#include "TextureUtils.h"
#include "DirectXResourceUtils.h"
#include "../../base/AssetPath.h"

#include <Windows.h>
#include <cassert>
#include <filesystem>

std::wstring ConvertString(const std::string &str) {
  if (str.empty()) {
    return std::wstring();
  }

  auto sizeNeeded =
      MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]),
                          static_cast<int>(str.size()), NULL, 0);
  if (sizeNeeded == 0) {
    return std::wstring();
  }
  std::wstring result(sizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]),
                      static_cast<int>(str.size()), &result[0], sizeNeeded);
  return result;
}

std::string ConvertString(const std::wstring &str) {
  if (str.empty()) {
    return std::string();
  }

  auto sizeNeeded =
      WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                          NULL, 0, NULL, NULL);
  if (sizeNeeded == 0) {
    return std::string();
  }
  std::string result(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), sizeNeeded, NULL, NULL);
  return result;
}

DirectX::ScratchImage LoadTexture(const std::string &filePath) {
  using namespace std;
  using namespace std::filesystem;
  DirectX::ScratchImage mipImages{};

  // exe があるフォルダ基準の実パスに解決する(他の場所へはフォールバックしない)
  path candidate = AbsoluteEngine::AssetPath::Resolve(filePath);

  if (!exists(candidate)) {
    std::string msg = "LoadTexture: file NOT found: " + filePath + "\n";
    OutputDebugStringA(msg.c_str());
    // 失敗時は空の画像を返す（assertで止めたいならここでassertでもOK）
    return mipImages;
  }

  DirectX::ScratchImage image{};
  HRESULT hr;
  // 拡張子でDDSかどうかを判断して読み込み方法を変える
  if (candidate.extension() == L".dds" || candidate.extension() == L".DDS") {
    hr = DirectX::LoadFromDDSFile(candidate.c_str(), DirectX::DDS_FLAGS_NONE,
                                  nullptr, image);
    if (FAILED(hr)) {
      std::string msg =
          "LoadTexture: LoadFromDDSFile failed: " + filePath + "\n";
      OutputDebugStringA(msg.c_str());
      return mipImages;
    }
  } else {
    hr = DirectX::LoadFromWICFile(
        candidate.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    if (FAILED(hr)) {
      std::string msg =
          "LoadTexture: LoadFromWICFile failed: " + filePath + "\n";
      OutputDebugStringA(msg.c_str());
      return mipImages;
    }
  }

  // 圧縮フォーマットかどうか調べる
  if (DirectX::IsCompressed(image.GetMetadata().format)) {
    mipImages = std::move(image);
  } else {
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                                  image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
                                  0, mipImages);
    if (FAILED(hr)) {
      std::string msg =
          "LoadTexture: GenerateMipMaps failed: " + filePath + "\n";
      OutputDebugStringA(msg.c_str());
      return DirectX::ScratchImage{};
    }
  }

  return mipImages;
}

ComPtr<ID3D12Resource>
CreateTextureResource(const ComPtr<ID3D12Device> &device,
                      const DirectX::TexMetadata &metadata) {
  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Width = UINT(metadata.width);
  resourceDesc.Height = UINT(metadata.height);
  resourceDesc.MipLevels = UINT16(metadata.mipLevels);
  // 3D テクスチャでは、この値は奥行き(スライス数)。1D/2D では配列の要素数
  resourceDesc.DepthOrArraySize = UINT16(metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D ? metadata.depth : metadata.arraySize);
  resourceDesc.Format = metadata.format;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

  D3D12_HEAP_PROPERTIES heapProperties{};
  heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProperties.CreationNodeMask = 1;
  heapProperties.VisibleNodeMask = 1;

  ComPtr<ID3D12Resource> resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));
  return resource;
}

// void UploadTextureData(const ComPtr<ID3D12Resource> &texture,
//                        const DirectX::ScratchImage &mipImages) {
//   const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
//   for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
//     const DirectX::Image *img = mipImages.GetImage(mipLevel, 0, 0);
//     HRESULT hr =
//         texture->WriteToSubresource(UINT(mipLevel), nullptr, img->pixels,
//                                     UINT(img->rowPitch),
//                                     UINT(img->slicePitch));
//     assert(SUCCEEDED(hr));
//   }
// }

[[nodiscard]]
ComPtr<ID3D12Resource>
UploadTextureData(const ComPtr<ID3D12Resource> &texture,
                  const DirectX::ScratchImage &mipImages,
                  const ComPtr<ID3D12Device> &device,
                  const ComPtr<ID3D12GraphicsCommandList> &commandList) {
  const DirectX::TexMetadata &metadata = mipImages.GetMetadata();

  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  subresources.reserve(
      static_cast<size_t>(metadata.mipLevels * metadata.arraySize));

  for (size_t item = 0; item < metadata.arraySize; ++item) {
    for (size_t mip = 0; mip < metadata.mipLevels; ++mip) {
      const DirectX::Image *img = mipImages.GetImage(mip, item, 0);
      assert(img != nullptr);

      D3D12_SUBRESOURCE_DATA subresource{};
      subresource.pData = img->pixels;
      subresource.RowPitch = static_cast<LONG_PTR>(img->rowPitch);
      subresource.SlicePitch = static_cast<LONG_PTR>(img->slicePitch);
      subresources.push_back(subresource);
    }
  }

  UINT64 intermediateSize = GetRequiredIntermediateSize(
      texture.Get(), 0, static_cast<UINT>(subresources.size()));

  ComPtr<ID3D12Resource> intermediateResource =
      CreateBufferResource(device, static_cast<size_t>(intermediateSize));

  UpdateSubresources(
      commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0,
      static_cast<UINT>(subresources.size()), subresources.data());

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = texture.Get();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

  commandList->ResourceBarrier(1, &barrier);

  return intermediateResource;
}