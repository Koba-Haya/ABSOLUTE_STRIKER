#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <string>

#include "SrvHandle.h"

namespace DirectX {
    class ScratchImage;
    struct TexMetadata;
} // namespace DirectX

class DirectXCommon;

// Texture2D + SRV を RAII で管理する
// - ID3D12Resource は ComPtr
// - SRV の index は SrvHandle が解放する
class TextureResource {
public:
    TextureResource() = default;
    // 破棄のとき、テクスチャと転送用バッファを、すぐには解放せず、そのフレームの GPU の実行が終わるまで預ける(DirectXCommon::DeferRelease)。
    // アップロードの命令がフレームの最後まで実行されないため、同じフレームの中で破棄されると、解放済みのメモリを GPU が読んでしまう
    ~TextureResource();
    TextureResource(TextureResource&&) = default;
    TextureResource& operator=(TextureResource&&) = default;
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;

    bool CreateFromFile(DirectXCommon* dx, const std::string& filePath);
    bool CreateFromMetadata(DirectXCommon* dx, const DirectX::ScratchImage& mipImages,
        const DirectX::TexMetadata& meta);

    ID3D12Resource* GetResource() const { return texture_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu() const { return srvGpu_; }

private:
    bool CreateSrv_(DirectXCommon* dx, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);

private:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D12Resource> texture_;
    ComPtr<ID3D12Resource> intermediateResource_;
    SrvHandle srv_;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu_{};

    // 調査用: 作られたときのフレーム番号と、ファイルのパス。作られたのと同じフレームで破棄されたら、ログに出す
    uint64_t createdFrame_ = 0;
    std::string debugName_;
};
