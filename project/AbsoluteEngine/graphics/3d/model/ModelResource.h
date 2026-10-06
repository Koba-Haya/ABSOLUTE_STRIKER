#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Vector.h"

struct ModelData;
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