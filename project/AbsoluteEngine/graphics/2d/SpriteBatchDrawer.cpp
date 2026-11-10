#include "SpriteBatchDrawer.h"
#include "Renderer.h"
#include "Sprite.h" // MaterialCB / TransformCB
#include <cassert>
#include <cstring>

namespace {
constexpr uint32_t kVerticesPerQuad = 4;
constexpr uint32_t kIndicesPerQuad = 6;
// クワッド内のインデックスの並び(Sprite の単位四角形と同じ)
constexpr uint16_t kQuadIndexPattern[kIndicesPerQuad] = {0, 1, 2, 2, 1, 3};

// 定数バッファは 256 バイト境界に置く決まり
constexpr uint32_t kConstantAlignment = 256;
constexpr uint32_t Align256(uint32_t n) {
  return (n + kConstantAlignment - 1) & ~(kConstantAlignment - 1);
}
constexpr uint32_t kMaterialSlotSize = Align256(sizeof(Sprite::MaterialCB));
constexpr uint32_t kTransformSlotSize = Align256(sizeof(Sprite::TransformCB));
// 描画 1 回ぶん(マテリアル + 変換)のバイト数
constexpr uint32_t kConstantSlotSize = kMaterialSlotSize + kTransformSlotSize;
} // namespace

void SpriteBatchDrawer::Initialize(Renderer *renderer) {
  // 頂点バッファ(毎フレーム書き換えるので Upload ヒープに置き、Map したままにする)
  vertexBuffer_ = renderer->CreateUploadBuffer(
      sizeof(SpriteVertex) * kVerticesPerQuad * kMaxQuads);
  vertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mappedVertices_));

  // インデックスバッファ(中身は固定なので、最初に全部書いて Unmap する)
  // 描画時は BaseVertexLocation で頂点の開始位置をずらすので、インデックスは 0 起点のままでよい
  indexBuffer_ = renderer->CreateUploadBuffer(sizeof(uint16_t) * kIndicesPerQuad * kMaxQuads);
  uint16_t *indices = nullptr;
  indexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&indices));
  for (uint32_t quad = 0; quad < kMaxQuads; ++quad) {
    for (uint32_t i = 0; i < kIndicesPerQuad; ++i) {
      indices[quad * kIndicesPerQuad + i] =
          static_cast<uint16_t>(quad * kVerticesPerQuad + kQuadIndexPattern[i]);
    }
  }
  indexBuffer_->Unmap(0, nullptr);

  // 定数バッファ(描画 kMaxDraws 回ぶんを 1 本にまとめる)
  constantBuffer_ = renderer->CreateUploadBuffer(static_cast<size_t>(kConstantSlotSize) * kMaxDraws);
  constantBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&mappedConstants_));
}

bool SpriteBatchDrawer::Submit(uint64_t frameCount, const SpriteVertex *vertices,
                               uint32_t quadCount, const Vector4 &color,
                               const Matrix4x4 &wvp, Submission *out) {
  if (!mappedVertices_ || !mappedConstants_ || !vertices || !out || quadCount == 0) {
    return false;
  }

  // フレームが変わったら、書き込み位置を先頭へ戻す。
  // GPU は毎フレーム描画の完了を待つので、前のフレームのデータはもう使われていない。
  if (frameCount != lastFrameCount_) {
    lastFrameCount_ = frameCount;
    usedQuads_ = 0;
    usedDraws_ = 0;
  }
  if (usedQuads_ + quadCount > kMaxQuads || usedDraws_ >= kMaxDraws) {
    return false; // 上限を超える分は描かない(あふれてもメモリは壊さない)
  }

  // 頂点を共有バッファの末尾へ追記
  std::memcpy(mappedVertices_ + static_cast<size_t>(usedQuads_) * kVerticesPerQuad, vertices,
              sizeof(SpriteVertex) * kVerticesPerQuad * quadCount);

  // 定数バッファの今回の組へ、色と行列を書く
  uint8_t *slot = mappedConstants_ + static_cast<size_t>(usedDraws_) * kConstantSlotSize;
  auto *material = reinterpret_cast<Sprite::MaterialCB *>(slot);
  material->color = color;
  material->uvTransform = MakeIdentity4x4();
  auto *transform = reinterpret_cast<Sprite::TransformCB *>(slot + kMaterialSlotSize);
  transform->WVP = wvp;
  transform->World = MakeIdentity4x4();

  const D3D12_GPU_VIRTUAL_ADDRESS constantBase = constantBuffer_->GetGPUVirtualAddress();
  const uint64_t slotOffset = static_cast<uint64_t>(usedDraws_) * kConstantSlotSize;
  out->vertexBufferView.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
  out->vertexBufferView.SizeInBytes = sizeof(SpriteVertex) * kVerticesPerQuad * kMaxQuads;
  out->vertexBufferView.StrideInBytes = sizeof(SpriteVertex);
  out->indexBufferView.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
  out->indexBufferView.SizeInBytes = sizeof(uint16_t) * kIndicesPerQuad * kMaxQuads;
  out->indexBufferView.Format = DXGI_FORMAT_R16_UINT;
  out->materialCB = constantBase + slotOffset;
  out->transformCB = constantBase + slotOffset + kMaterialSlotSize;
  out->indexCount = quadCount * kIndicesPerQuad;
  out->baseVertex = static_cast<int32_t>(usedQuads_ * kVerticesPerQuad);

  usedQuads_ += quadCount;
  ++usedDraws_;
  return true;
}
