#include "ShieldDome.h"
#include "Method.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
// 定数バッファは 256 バイト単位で確保する(D3D12 の決まり)
constexpr UINT64 kConstantBufferAlignment = 256;
// 球を細かくする回数(正二十面体の 20 面を、1 回ごとに 4 倍に割る)。2 回で 320 面
constexpr int kSubdivisionLevels = 2;
// 正二十面体の頂点の座標を作る黄金比
constexpr float kGoldenRatio = 1.61803398875f;
// 砕ける進み具合の上限(最後の面が、飛び終えるまで進められるよう、1 + 破片の長さ(shardSpan の最大)の余裕を持たせる)
constexpr float kMaxBreakProgress = 2.0f;
// 面ごとの乱数を作る、整数の混ぜ方(隣り合う面の値が、似すぎないようにする)
constexpr uint32_t kHashMultiplierA = 747796405u;
constexpr uint32_t kHashMultiplierB = 2891336453u;
constexpr uint32_t kHashShift = 22u;
constexpr float kHashRange = 4294967295.0f;

Vector3 NormalizeVec(const Vector3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return { v.x / len, v.y / len, v.z / len };
}

Vector3 MidPoint(const Vector3& a, const Vector3& b) {
    return NormalizeVec({ (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f });
}

// 面の番号から、0 〜 1 の乱数を作る(同じ番号なら、いつも同じ値)
float FaceRandom(uint32_t index) {
    uint32_t state = index * kHashMultiplierA + 1u;
    uint32_t word = ((state >> ((state >> kHashShift) + 4u)) ^ state) * kHashMultiplierB;
    word = (word >> kHashShift) ^ word;
    return static_cast<float>(word) / kHashRange;
}
}

Microsoft::WRL::ComPtr<ID3D12Resource> ShieldDome::CreateUploadBuffer_(ID3D12Device* device, UINT64 size) {
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    const HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));
    if (FAILED(hr)) return nullptr;
    return buffer;
}

bool ShieldDome::Initialize(ID3D12Device* device) {
    if (!device) return false;

    // --- 球のメッシュ: 正二十面体を、再帰的に割って、面ごとに頂点を分けて並べる ---
    const float t = kGoldenRatio;
    const Vector3 icoVertices[12] = {
        { -1,  t, 0 }, { 1,  t, 0 }, { -1, -t, 0 }, { 1, -t, 0 },
        { 0, -1,  t }, { 0, 1,  t }, { 0, -1, -t }, { 0, 1, -t },
        {  t, 0, -1 }, { t, 0,  1 }, { -t, 0, -1 }, { -t, 0, 1 },
    };
    const int icoFaces[20][3] = {
        { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
        { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
        { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
        { 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 },
    };

    std::vector<Vertex> vertices;
    uint32_t faceIndex = 0;
    // 三角形 (a, b, c) を level 回割って、できた三角形を、頂点として追加する
    auto subdivide = [&](auto&& self, const Vector3& a, const Vector3& b, const Vector3& c, int level) -> void {
        if (level == 0) {
            const float random = FaceRandom(faceIndex++);
            const Vector3 center = { (a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f };
            vertices.push_back({ a, { 1.0f, 0.0f, 0.0f }, random, center });
            vertices.push_back({ b, { 0.0f, 1.0f, 0.0f }, random, center });
            vertices.push_back({ c, { 0.0f, 0.0f, 1.0f }, random, center });
            return;
        }
        const Vector3 ab = MidPoint(a, b);
        const Vector3 bc = MidPoint(b, c);
        const Vector3 ca = MidPoint(c, a);
        self(self, a, ab, ca, level - 1);
        self(self, b, bc, ab, level - 1);
        self(self, c, ca, bc, level - 1);
        self(self, ab, bc, ca, level - 1);
    };
    for (const auto& face : icoFaces) {
        subdivide(subdivide, NormalizeVec(icoVertices[face[0]]), NormalizeVec(icoVertices[face[1]]), NormalizeVec(icoVertices[face[2]]), kSubdivisionLevels);
    }
    vertexCount_ = static_cast<UINT>(vertices.size());

    const UINT64 vertexBytes = sizeof(Vertex) * vertices.size();
    vertexBuffer_ = CreateUploadBuffer_(device, vertexBytes);
    transformCB_ = CreateUploadBuffer_(device, (sizeof(TransformCB) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1));
    paramsCB_ = CreateUploadBuffer_(device, (sizeof(ParamsCB) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1));
    if (!IsReady()) return false;

    void* mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, vertices.data(), vertexBytes);
    vertexBuffer_->Unmap(0, nullptr);
    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = static_cast<UINT>(vertexBytes);
    vbView_.StrideInBytes = sizeof(Vertex);
    return true;
}

void ShieldDome::SetParams(const Params& params) {
    if (!paramsCB_) return;
    ParamsCB cb{};
    cb.baseColor = params.baseColor;
    cb.lineColor = params.lineColor;
    cb.damageColor = params.damageColor;
    cb.rippleColor = params.rippleColor;
    cb.cameraPosition = params.cameraPosition;
    cb.time = params.time;
    cb.hpRatio = std::clamp(params.hpRatio, 0.0f, 1.0f);
    cb.breakProgress = std::clamp(params.breakProgress, 0.0f, kMaxBreakProgress);
    // 砕ける値は、頂点シェーダーが読む変換の定数バッファへ、SetTransform が書く
    breakProgress_ = cb.breakProgress;
    shardSpan_ = params.shardSpan;
    shardDistance_ = params.shardDistance;
    shardShrink_ = params.shardShrink;
    cb.appear = std::clamp(params.appear, 0.0f, 1.0f);
    cb.warning = std::clamp(params.warning, 0.0f, 1.0f);
    cb.alpha = std::clamp(params.alpha, 0.0f, 1.0f);
    cb.lineWidth = params.lineWidth;
    cb.fresnelPower = params.fresnelPower;
    const uint32_t hitCount = (std::min)(params.hitCount, kMaxHits);
    cb.hitCount = static_cast<float>(hitCount);
    cb.rippleSpeed = params.rippleSpeed;
    cb.rippleWidth = params.rippleWidth;
    cb.rippleLife = params.rippleLife;
    for (uint32_t i = 0; i < hitCount; ++i) {
        cb.hits[i] = { params.hits[i].direction.x, params.hits[i].direction.y, params.hits[i].direction.z, params.hits[i].age };
    }

    void* mapped = nullptr;
    paramsCB_->Map(0, nullptr, &mapped);
    if (mapped) {
        std::memcpy(mapped, &cb, sizeof(cb));
        paramsCB_->Unmap(0, nullptr);
    }
}

void ShieldDome::SetTransform(const Vector3& center, float radius, const Matrix4x4& view, const Matrix4x4& proj) {
    if (!transformCB_) return;
    // 拡大してから、中心へ平行移動する(回転はしない)
    const Matrix4x4 world = Multiply(MakeScaleMatrix(Vector3{ radius, radius, radius }), MakeTranslateMatrix(center));
    void* mapped = nullptr;
    transformCB_->Map(0, nullptr, &mapped);
    if (mapped) {
        TransformCB* cb = static_cast<TransformCB*>(mapped);
        cb->World = world;
        cb->WVP = Multiply(Multiply(world, view), proj);
        cb->shatter = { breakProgress_, shardSpan_, shardDistance_, shardShrink_ };
        transformCB_->Unmap(0, nullptr);
    }
}

void ShieldDome::Draw(ID3D12GraphicsCommandList* cmdList) {
    if (!IsReady() || !cmdList) return;
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(vertexCount_, 1, 0, 0);
}
