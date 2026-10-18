#include "TextureResource.h"

#include <Windows.h>
#include <filesystem>
#include <string>

#include "DirectXCommon.h"
#include "SrvAllocator.h"
#include "TextureUtils.h"
#include "externals/DirectXTex/DirectXTex.h"

static void TexResLog_(const std::string &msg) {
  OutputDebugStringA((msg + "\n").c_str());
}

bool TextureResource::CreateFromFile(DirectXCommon *dx,
                                     const std::string &filePath) {
  if (!dx) {
    TexResLog_("[TextureResource] dx is null");
    return false;
  }

  DirectX::ScratchImage mipImages = LoadTexture(filePath);
  const DirectX::TexMetadata &meta = mipImages.GetMetadata();

  if (mipImages.GetImageCount() == 0) {
    TexResLog_(std::string("[TextureResource] failed to load texture: ") +
               filePath);
    return false;
  }

  if (!CreateFromMetadata(dx, mipImages, meta)) {
    return false;
  }

  // クラッシュ時にVisual Studioの出力/ライブオブジェクトウィンドウで
  // "Unnamed Object" ではなくファイルパスが表示されるようにデバッグ名を付ける。
  if (texture_) {
    const std::wstring wName(filePath.begin(), filePath.end());
    texture_->SetName(wName.c_str());
  }
  if (intermediateResource_) {
    const std::wstring wUploadName = L"Upload:" + std::wstring(filePath.begin(), filePath.end());
    intermediateResource_->SetName(wUploadName.c_str());
  }

  return true;
}

bool TextureResource::CreateFromMetadata(DirectXCommon *dx,
                                         const DirectX::ScratchImage &mipImages,
                                         const DirectX::TexMetadata &meta) {
  if (!dx) {
    TexResLog_("[TextureResource] dx is null");
    return false;
  }

  ID3D12Device *rawDevice = dx->GetDevice();
  ID3D12GraphicsCommandList *rawCommandList = dx->GetCommandList();

  if (!rawDevice) {
    TexResLog_("[TextureResource] device is null");
    return false;
  }

  if (!rawCommandList) {
    TexResLog_("[TextureResource] commandList is null");
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D12Device> device(rawDevice);
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList(rawCommandList);

  texture_ = CreateTextureResource(device, meta);
  if (!texture_) {
    TexResLog_("[TextureResource] CreateTextureResource failed");
    return false;
  }

  intermediateResource_ =
      UploadTextureData(texture_, mipImages, device, commandList);
  if (!intermediateResource_) {
    TexResLog_("[TextureResource] UploadTextureData failed");
    return false;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = meta.format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  if (meta.IsCubemap()) {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = UINT_MAX;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
  } else {

    if (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE1D) {
      if (meta.arraySize > 1) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
        srvDesc.Texture1DArray.MipLevels = static_cast<UINT>(meta.mipLevels);
        srvDesc.Texture1DArray.ArraySize = static_cast<UINT>(meta.arraySize);
      } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
        srvDesc.Texture1D.MipLevels = static_cast<UINT>(meta.mipLevels);
      }
    } else if (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
      if (meta.arraySize > 1) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = static_cast<UINT>(meta.mipLevels);
        srvDesc.Texture2DArray.ArraySize = static_cast<UINT>(meta.arraySize);
      } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(meta.mipLevels);
      }
    } else if (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE3D) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MipLevels = static_cast<UINT>(meta.mipLevels);
    } else {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = static_cast<UINT>(meta.mipLevels);
    }
  }

  return CreateSrv_(dx, srvDesc);
}

bool TextureResource::CreateSrv_(DirectXCommon *dx,
                                 const D3D12_SHADER_RESOURCE_VIEW_DESC &desc) {
  if (!dx) {
    return false;
  }

  ID3D12Device *device = dx->GetDevice();
  if (!device) {
    return false;
  }

  if (!texture_) {
    TexResLog_("[TextureResource] texture_ is null when creating SRV");
    return false;
  }

  SrvAllocator &alloc = dx->GetSrvAllocator();
  const UINT index = alloc.Allocate();

  device->CreateShaderResourceView(texture_.Get(), &desc, alloc.Cpu(index));

  srv_ = SrvHandle(&alloc, index);
  srvGpu_ = alloc.Gpu(index);
  return true;
}