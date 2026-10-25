#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Vector.h"

struct ModelData;
struct VertexData;
struct SubMeshRange;
class TextureResource;
class DirectXCommon;

class ModelResource {
public:
  struct CreateInfo {
    DirectXCommon *dx = nullptr;
    std::shared_ptr<const ModelData> modelData;
    std::shared_ptr<TextureResource> texture;
  };

  ModelResource();
  ~ModelResource(); // 実装は .cpp へ

  ModelResource(const ModelResource &) = delete;
  ModelResource &operator=(const ModelResource &) = delete;

  bool Initialize(const CreateInfo &ci);

  const ModelData* GetModelData() const;

  // 頂点バッファの内容を書き換える。毎フレーム形が変わるメッシュ(砕けて飛ぶ破片など)用。
  // 頂点の数は、作ったとき(Initialize)と同じでなければならない(違うときは何もせず false を返す)。
  // 位置の更新だけでなく、法線や UV も書き換えられる。インデックスは変わらない
  // vertices: 新しい頂点(count 個)。count: 頂点の数
  bool UpdateVertices(const VertexData *vertices, uint32_t count);

  // 三角形の番号(インデックスバッファ)の内容を書き換える。一部の三角形を描かないようにする(番号を潰す)用途。
  // 番号の数は、作ったときと同じでなければならない(違うときは何もせず false を返す)。
  // 注意: モデルのリソースは、同じファイルを読むすべての場所で共有される。書き換えたら、使い終わったときに元へ戻すこと
  bool UpdateIndices(const uint32_t *indices, uint32_t count);

  // 全メッシュの頂点位置(モデル空間)を返す。モデルの大きさ・中心・端の位置を調べる用途(Assimp の型に依存しない)
  std::vector<Vector3> GetVertexPositions() const;

  // Renderer が使用するアクセサ
  unsigned long long GetVBVAddress() const;
  unsigned int GetVBVSize() const;
  unsigned int GetVBVStride() const;
  uint32_t GetVertexCount() const;

  unsigned long long GetIBVAddress() const;
  unsigned int GetIBVSize() const;
  uint32_t GetIndexCount() const;

  unsigned long long GetTextureHandleGPUAsUInt64() const;

  // MultiMesh & MultiMaterial対応。単一メッシュのモデルでは0を返し、
  // 呼び出し側は従来通りの単一描画パスにフォールバックする。
  uint32_t GetSubMeshCount() const;
  const SubMeshRange &GetSubMeshRange(uint32_t index) const;
  unsigned long long GetSubMeshTextureHandleGPUAsUInt64(uint32_t index) const;

  bool HasBones() const;
  unsigned long long GetBoneVBVAddress() const;
  unsigned int GetBoneVBVSize() const;
  unsigned int GetBoneVBVStride() const;

  uint32_t GetVertexSRVIndex() const;
  uint32_t GetBoneSRVIndex() const;

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl_;
};