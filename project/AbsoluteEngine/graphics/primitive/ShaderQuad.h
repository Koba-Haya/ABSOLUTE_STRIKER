#pragma once
#include "Matrix.h"
#include "Vector.h"
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

// 板(正方形のポリゴン) 1 枚と、その変換行列とパラメータの定数バッファを持つ部品。
// 板の上の絵は、ピクセルシェーダーが計算して描く(テクスチャは使わない)。
//
// 分けた理由: 「板 1 枚 + シェーダー」で描く演出(EnergyOrb など)が増えるたびに、板の頂点、
// 定数バッファ、ビルボードの行列の作り方を、コピーして書かないようにするため。
// 演出ごとに違うのは、パラメータの中身(各演出のクラスが、並びを決めて渡す)だけ。
// 板の頂点シェーダーは、ArcRing.VS.hlsl と共通(板の上の座標 -1 〜 +1 を、ピクセルシェーダーへ渡す)。
//
// 使い方: Initialize(1 回。パラメータのバイト数を渡す) -> 毎フレーム SetParamsData と SetBillboardTransform -> Renderer が Draw
class ShaderQuad {
public:
    ShaderQuad() = default;
    ~ShaderQuad() = default;

    // 板と定数バッファを作る。デバイスから 1 回だけ呼ぶ。失敗したら false
    // paramsByteSize: ピクセルシェーダーへ渡すパラメータのバイト数(定数バッファの確保に使う)
    bool Initialize(ID3D12Device* device, std::size_t paramsByteSize);

    // パラメータを、定数バッファへ書く。byteSize が確保した大きさを超えるときは、書かない
    void SetParamsData(const void* data, std::size_t byteSize);

    // 変換行列を直接決める(world: 板のワールド行列。板は XY 平面、-1 〜 +1 の正方形)
    void SetTransform(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj);

    // 板を、カメラのほうへ向けて置く(ビルボード)。position: 板の中心(ワールド)。worldRadius: 板の半分の長さ(ワールド)
    void SetBillboardTransform(const Vector3& position, float worldRadius, const Matrix4x4& view, const Matrix4x4& proj);

    // 描画の前準備が済んでいるか(Initialize に成功したか)
    bool IsReady() const { return vertexBuffer_ && indexBuffer_ && transformCB_ && paramsCB_; }

    void Draw(ID3D12GraphicsCommandList* cmdList);

    D3D12_GPU_VIRTUAL_ADDRESS GetParamsCBAddress() const { return paramsCB_->GetGPUVirtualAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformCBAddress() const { return transformCB_->GetGPUVirtualAddress(); }

private:
    struct Vertex {
        Vector4 position;
        Vector2 texcoord; // 板の中心を原点とした、-1 〜 +1 の座標(ピクセルシェーダーが、絵の計算に使う)
    };

    struct TransformCB {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer_(ID3D12Device* device, UINT64 size);

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> transformCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsCB_;
    std::size_t paramsByteSize_ = 0; // 確保したパラメータの大きさ(書き込みの上限)
};
