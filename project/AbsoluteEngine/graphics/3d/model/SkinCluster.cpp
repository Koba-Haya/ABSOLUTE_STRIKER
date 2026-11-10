#include "SkinCluster.h"
#include "ModelUtils.h"
#include "Method.h"
#include "DirectXCommon.h"
#include "Renderer.h"

bool SkinCluster::Initialize(DirectXCommon* dx, uint32_t jointCount, uint32_t vertexCount) {
  auto* device = dx->GetDevice();
  auto& srvAlloc = dx->GetSrvAllocator();

  size_t paletteSize = sizeof(WellForGPU) * jointCount;
  paletteResource = Renderer::GetInstance()->CreateBuffer(paletteSize);
  if (!paletteResource) return false;

  HRESULT hr = paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
  if (FAILED(hr)) return false;

  // Initialize with identity
  for (uint32_t i = 0; i < jointCount; ++i) {
    mappedPalette[i].skeletonSpaceMatrix = MakeIdentity4x4();
    mappedPalette[i].skeletonSpaceInverseTransposeMatrix = MakeIdentity4x4();
  }

  // Create SRV for Palette
  srvIndex = srvAlloc.Allocate();
  
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = DXGI_FORMAT_UNKNOWN;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Buffer.FirstElement = 0;
  srvDesc.Buffer.NumElements = jointCount;
  srvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
  srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

  device->CreateShaderResourceView(paletteResource.Get(), &srvDesc, srvAlloc.Cpu(srvIndex));

  // --- CS の定数バッファ(頂点数)。値は変わらないので、作るときに 1 回だけ書く ---
  // 定数バッファのサイズは 256 バイトの倍数にする
  constexpr size_t kConstantBufferAlignment = 256;
  infoResource = Renderer::GetInstance()->CreateUploadBuffer(kConstantBufferAlignment);
  if (!infoResource) return false;
  uint32_t* mappedInfo = nullptr;
  if (FAILED(infoResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfo)))) return false;
  mappedInfo[0] = vertexCount;
  infoResource->Unmap(0, nullptr);

  // --- CS出力用バッファの生成 ---
  size_t vbSize = sizeof(VertexData) * vertexCount;
  skinnedVertexBuffer = Renderer::GetInstance()->CreateUAVBuffer(vbSize);
  if (!skinnedVertexBuffer) return false;

  // UAVの作成
  uavIndex = srvAlloc.Allocate();
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = vertexCount;
  uavDesc.Buffer.StructureByteStride = sizeof(VertexData);
  uavDesc.Buffer.CounterOffsetInBytes = 0;
  uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

  device->CreateUnorderedAccessView(skinnedVertexBuffer.Get(), nullptr, &uavDesc, srvAlloc.Cpu(uavIndex));

  // VBVの設定
  vbView.BufferLocation = skinnedVertexBuffer->GetGPUVirtualAddress();
  vbView.SizeInBytes = static_cast<UINT>(vbSize);
  vbView.StrideInBytes = sizeof(VertexData);

  return true;
}

void SkinCluster::Update(const Skeleton& skeleton) {
  if (!mappedPalette) return;

  // Update palette
  for (size_t i = 0; i < skeleton.joints.size(); ++i) {
    const Joint& joint = skeleton.joints[i];
    Matrix4x4 finalMatrix = Multiply(joint.inverseBindPoseMatrix, joint.skeletonSpaceMatrix);
    mappedPalette[i].skeletonSpaceMatrix = finalMatrix;
    mappedPalette[i].skeletonSpaceInverseTransposeMatrix = Transpose(Inverse(finalMatrix));
  }
}
