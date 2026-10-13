#include "ModelResource.h"
#include "ModelUtils.h"
#include "Renderer.h"
#include "DirectXCommon.h"
#include "../../../resources/AssetManager.h"
#include "TextureResource.h"
#include <cassert>
#include <d3d12.h>
#include <wrl/client.h>

struct ModelResource::Impl {
  Microsoft::WRL::ComPtr<ID3D12Resource> vb;
  D3D12_GPU_VIRTUAL_ADDRESS vbAddress = 0;
  unsigned int vbSize = 0;
  unsigned int vbStride = 0;
  uint32_t vertexCount = 0;
  
  Microsoft::WRL::ComPtr<ID3D12Resource> ib;
  D3D12_GPU_VIRTUAL_ADDRESS ibAddress = 0;
  unsigned int ibSize = 0;
  uint32_t indexCount = 0;
  
  Microsoft::WRL::ComPtr<ID3D12Resource> vbBone;
  D3D12_GPU_VIRTUAL_ADDRESS vbBoneAddress = 0;
  unsigned int vbBoneSize = 0;
  unsigned int vbBoneStride = 0;
  bool hasBones = false;

  uint32_t vertexSrvIndex = 0;
  uint32_t boneSrvIndex = 0;
  
  std::shared_ptr<const ModelData> modelData;
  std::shared_ptr<TextureResource> texture;

  // MultiMesh & MultiMaterial対応（meshes.size() > 1のモデルのみ設定される）
  std::vector<SubMeshRange> subMeshRanges;
  std::vector<std::shared_ptr<TextureResource>> subMeshTextures;
};

ModelResource::ModelResource() : pImpl_(std::make_unique<Impl>()) {}
ModelResource::~ModelResource() = default;

bool ModelResource::Initialize(const CreateInfo &ci) {
  assert(ci.modelData);
  pImpl_->modelData = ci.modelData;
  auto *renderer = Renderer::GetInstance();

  const std::vector<VertexData> vertices = FlattenVertices(*ci.modelData);
  if (vertices.empty())
    return false;

  pImpl_->vertexCount = static_cast<uint32_t>(vertices.size());
  const size_t vbBufferSize = sizeof(VertexData) * vertices.size();

  pImpl_->vb = renderer->CreateUploadBuffer(vbBufferSize);
  if (!pImpl_->vb)
    return false;

  void *mapped = nullptr;
  if (SUCCEEDED(pImpl_->vb->Map(0, nullptr, &mapped))) {
    std::memcpy(mapped, vertices.data(), vbBufferSize);
    pImpl_->vb->Unmap(0, nullptr);
  }

  pImpl_->vbAddress = pImpl_->vb->GetGPUVirtualAddress();
  pImpl_->vbSize = static_cast<unsigned int>(vbBufferSize);
  pImpl_->vbStride = sizeof(VertexData);
  pImpl_->vb->SetName(L"ModelResource::VertexBuffer");

  // Vertex SRV
  {
    auto& srvAlloc = ci.dx->GetSrvAllocator();
    pImpl_->vertexSrvIndex = srvAlloc.Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = pImpl_->vertexCount;
    srvDesc.Buffer.StructureByteStride = sizeof(VertexData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    ci.dx->GetDevice()->CreateShaderResourceView(pImpl_->vb.Get(), &srvDesc, srvAlloc.Cpu(pImpl_->vertexSrvIndex));
  }

  // Index Buffer
  const std::vector<uint32_t> indices = FlattenIndices(*ci.modelData);
  if (!indices.empty()) {
    pImpl_->indexCount = static_cast<uint32_t>(indices.size());
    const size_t ibBufferSize = sizeof(uint32_t) * indices.size();
    pImpl_->ib = renderer->CreateUploadBuffer(ibBufferSize);
    if (pImpl_->ib) {
      void *ibMapped = nullptr;
      if (SUCCEEDED(pImpl_->ib->Map(0, nullptr, &ibMapped))) {
        std::memcpy(ibMapped, indices.data(), ibBufferSize);
        pImpl_->ib->Unmap(0, nullptr);
      }
      pImpl_->ibAddress = pImpl_->ib->GetGPUVirtualAddress();
      pImpl_->ibSize = static_cast<unsigned int>(ibBufferSize);
      pImpl_->ib->SetName(L"ModelResource::IndexBuffer");
    }
  }

  // MultiMesh & MultiMaterial対応：元のメッシュが複数ある場合のみサブメッシュ情報を構築する。
  // 単一メッシュのモデル（大半の既存アセット）は subMeshRanges が空のままとなり、
  // Renderer::DrawModel 側で従来通りの単一描画にフォールバックする。
  if (ci.modelData->meshes.size() > 1) {
    pImpl_->subMeshRanges = ComputeSubMeshRanges(*ci.modelData);
    pImpl_->subMeshTextures.reserve(pImpl_->subMeshRanges.size());
    for (const auto &range : pImpl_->subMeshRanges) {
      std::string texPath;
      if (range.materialIndex >= 0 &&
          static_cast<size_t>(range.materialIndex) < ci.modelData->materials.size()) {
        texPath = ci.modelData->materials[range.materialIndex].textureFilePath;
      }
      auto tex = AbsoluteEngine::AssetManager::GetInstance()->Load<TextureResource>(
          texPath.empty() ? "resources/engine/textures/uvChecker.png" : texPath);
      // 指定テクスチャの読み込みに失敗した場合、SRVハンドルが0のまま
      // SetGraphicsRootDescriptorTableに渡るとD3D12がクラッシュするため、
      // 必ず存在するエンジン既定のフォールバックテクスチャに差し替える。
      if (!tex || tex->GetSrvGpu().ptr == 0) {
        tex = AbsoluteEngine::AssetManager::GetInstance()->Load<TextureResource>(
            "resources/engine/textures/uvChecker.png");
      }
      pImpl_->subMeshTextures.push_back(tex);
    }
  }

  const std::vector<VertexBoneData> skinningData = FlattenSkinningData(*ci.modelData);
  if (!skinningData.empty() && skinningData.size() == vertices.size()) {
    pImpl_->hasBones = true;
    const size_t vbBoneBufferSize = sizeof(VertexBoneData) * skinningData.size();
    pImpl_->vbBone = renderer->CreateUploadBuffer(vbBoneBufferSize);
    if (pImpl_->vbBone) {
      void *boneMapped = nullptr;
      if (SUCCEEDED(pImpl_->vbBone->Map(0, nullptr, &boneMapped))) {
        std::memcpy(boneMapped, skinningData.data(), vbBoneBufferSize);
        pImpl_->vbBone->Unmap(0, nullptr);
      }
      pImpl_->vbBoneAddress = pImpl_->vbBone->GetGPUVirtualAddress();
      pImpl_->vbBoneSize = static_cast<unsigned int>(vbBoneBufferSize);
      pImpl_->vbBoneStride = sizeof(VertexBoneData);
      pImpl_->vbBone->SetName(L"ModelResource::BoneVertexBuffer");

      // Bone SRV
      auto& srvAlloc = ci.dx->GetSrvAllocator();
      pImpl_->boneSrvIndex = srvAlloc.Allocate();
      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
      srvDesc.Format = DXGI_FORMAT_UNKNOWN;
      srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      srvDesc.Buffer.FirstElement = 0;
      srvDesc.Buffer.NumElements = static_cast<uint32_t>(skinningData.size());
      srvDesc.Buffer.StructureByteStride = sizeof(VertexBoneData);
      srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
      ci.dx->GetDevice()->CreateShaderResourceView(pImpl_->vbBone.Get(), &srvDesc, srvAlloc.Cpu(pImpl_->boneSrvIndex));
    }
  }

  if (ci.texture) {
    pImpl_->texture = ci.texture;
  } else {
    const std::string texPath = PickDiffuseTexturePath(*ci.modelData);
    pImpl_->texture = AbsoluteEngine::AssetManager::GetInstance()->Load<TextureResource>(
        texPath.empty() ? "resources/engine/textures/uvChecker.png" : texPath);
  }

  return true;
}

unsigned long long ModelResource::GetVBVAddress() const {
  return pImpl_->vbAddress;
}
unsigned int ModelResource::GetVBVSize() const { return pImpl_->vbSize; }
unsigned int ModelResource::GetVBVStride() const { return pImpl_->vbStride; }
uint32_t ModelResource::GetVertexCount() const { return pImpl_->vertexCount; }

unsigned long long ModelResource::GetIBVAddress() const {
  return pImpl_->ibAddress;
}
unsigned int ModelResource::GetIBVSize() const { return pImpl_->ibSize; }
uint32_t ModelResource::GetIndexCount() const { return pImpl_->indexCount; }

const ModelData* ModelResource::GetModelData() const { return pImpl_->modelData.get(); }

std::vector<Vector3> ModelResource::GetVertexPositions() const {
  std::vector<Vector3> positions;
  if (!pImpl_->modelData) return positions;
  for (const MeshData &mesh : pImpl_->modelData->meshes) {
    for (const VertexData &v : mesh.vertices) {
      positions.push_back({v.position.x, v.position.y, v.position.z});
    }
  }
  return positions;
}

bool ModelResource::HasBones() const { return pImpl_->hasBones; }
unsigned long long ModelResource::GetBoneVBVAddress() const { return pImpl_->vbBoneAddress; }
unsigned int ModelResource::GetBoneVBVSize() const { return pImpl_->vbBoneSize; }
unsigned int ModelResource::GetBoneVBVStride() const { return pImpl_->vbBoneStride; }

uint32_t ModelResource::GetVertexSRVIndex() const { return pImpl_->vertexSrvIndex; }
uint32_t ModelResource::GetBoneSRVIndex() const { return pImpl_->boneSrvIndex; }

unsigned long long ModelResource::GetTextureHandleGPUAsUInt64() const {
  return pImpl_->texture ? pImpl_->texture->GetSrvGpu().ptr : 0;
}

uint32_t ModelResource::GetSubMeshCount() const {
  return static_cast<uint32_t>(pImpl_->subMeshRanges.size());
}

const SubMeshRange &ModelResource::GetSubMeshRange(uint32_t index) const {
  return pImpl_->subMeshRanges[index];
}

unsigned long long ModelResource::GetSubMeshTextureHandleGPUAsUInt64(uint32_t index) const {
  const auto &tex = pImpl_->subMeshTextures[index];
  return tex ? tex->GetSrvGpu().ptr : 0;
}