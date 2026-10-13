#pragma once

#include <cstdint>
#include <d3d12.h>

#include "DepthTexture.h"
#include "Matrix.h"
#include "Method.h"
#include "RenderTexture.h"

class DirectXCommon;

// 平面反射(水面など、水平な鏡)の鏡像を描くためのオフスクリーン描画先。
//
// 仕組み: カメラは本物のまま動かさず、反射させたい物体を「水面の高さで上下反転(鏡映)」して描く。
// 鏡映した物体を本物のカメラで見ると、水面に映った像と同じ位置に写るので、
// このテクスチャは本編の画面と同じ画面座標で対応する(水面のシェーダーが同じ UV でサンプルできる)。
//
// 使い方: Begin() → 反射させたい物体の描画 → End() を、水面を描く前に毎フレーム行う。
// 背景は透明(rgba = 0)で、物体を描いた所だけ α = 1 になる。
class PlanarReflection {
public:
    // 描画先(色 + 深度)を作る。width / height は本編の描画先と同じ大きさにする(画面座標が揃うため)
    void Initialize(DirectXCommon* dx, uint32_t width, uint32_t height);

    // 反射パスを始める。描画先を切り替えて透明で塗りつぶし、鏡映行列を準備する。
    // planeHeight: 鏡になる水平面のワールド y
    void Begin(DirectXCommon* dx, float planeHeight);

    // 反射パスを終える。描画先をバックバッファへ戻し、テクスチャを読める状態にする
    void End(DirectXCommon* dx);

    bool IsInitialized() const { return initialized_; }
    // Begin() と End() の間だけ true
    bool IsActive() const { return active_; }

    // 物体のワールド行列の右から掛けると、水面を挟んで上下反転した位置になる行列(行ベクトル規約)
    const Matrix4x4& GetMirrorMatrix() const { return mirrorMatrix_; }

    // 水面のシェーダーが t0 に束ねる、反射テクスチャの SRV
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return color_.GetSrvGpuHandle(); }

private:
    RenderTexture color_;
    DepthTexture depth_;
    Matrix4x4 mirrorMatrix_ = MakeIdentity4x4();
    bool initialized_ = false;
    bool active_ = false;
};
