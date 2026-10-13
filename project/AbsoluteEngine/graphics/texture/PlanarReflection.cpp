#include "PlanarReflection.h"
#include "DirectXCommon.h"
#include "Method.h"
#include <cassert>

namespace {
// 背景は完全な透明にする(物体の無い所は、水面側の空の反射がそのまま見える)
constexpr float kClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kClearDepth = 1.0f;
} // namespace

void PlanarReflection::Initialize(DirectXCommon* dx, uint32_t width, uint32_t height) {
    assert(dx);
    // 色は本編の描画先と同じ sRGB 形式。シェーダーで読むとリニア値になる
    color_.Initialize(dx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                      {kClearColor[0], kClearColor[1], kClearColor[2], kClearColor[3]});
    depth_.Initialize(dx, width, height);
    initialized_ = true;
}

void PlanarReflection::Begin(DirectXCommon* dx, float planeHeight) {
    assert(initialized_ && !active_);

    // 鏡映: y を反転してから 2h だけ持ち上げると、y = h を軸に折り返した位置になる(y' = 2h - y)。
    // 行ベクトル規約なので、左から順に「反転 → 平行移動」
    mirrorMatrix_ = Multiply(MakeScaleMatrix({1.0f, -1.0f, 1.0f}), MakeTranslateMatrix({0.0f, 2.0f * planeHeight, 0.0f}));

    dx->SetRenderTargetWithDepth(&color_, &depth_);
    auto* cmdList = dx->GetCommandList();
    cmdList->ClearRenderTargetView(color_.GetRtvHandle(), kClearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(depth_.GetDsvHandle(), D3D12_CLEAR_FLAG_DEPTH, kClearDepth, 0, 0, nullptr);
    active_ = true;
}

void PlanarReflection::End(DirectXCommon* dx) {
    assert(initialized_ && active_);
    dx->FinishRenderingWithDepth(&color_, &depth_);
    active_ = false;
}
