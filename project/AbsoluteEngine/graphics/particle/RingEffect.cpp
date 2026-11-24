#include "RingEffect.h"
#include "base/DirectXCommon.h"
#include "camera/Camera.h"
#include "graphics/Renderer.h"
#include "graphics/texture/TextureResource.h"
#include "math/Method.h"

RingEffect::RingEffect(std::shared_ptr<TextureResource> texture, const Vector3& position)
    : BaseEffect(kLifetime, position), texture_(std::move(texture)) {
    ringParams_.divide = 32;
    ringParams_.innerRadius = 0.0f;
    ringParams_.outerRadius = 1.0f;
    ringParams_.colorOuter = {1.0f, 1.0f, 1.0f, 1.0f};
    ringParams_.colorInner = {1.0f, 1.0f, 1.0f, 1.0f};
    ringParams_.uvVertical = true; // 放射状テクスチャを想定
    
    ring_.Initialize(Renderer::GetInstance()->GetDX()->GetDevice(), ringParams_);
}

void RingEffect::OnUpdate(float t, float /*deltaTime*/, const Camera* camera) {
    // 時間経過で拡大し、透明になる
    ringParams_.outerRadius = 1.0f + (maxOuterRadius_ - 1.0f) * t;
    alpha_ = 1.0f - t;
    ringParams_.colorInner.w = alpha_;
    ringParams_.colorOuter.w = alpha_;

    ring_.Update(Renderer::GetInstance()->GetDX()->GetDevice(), ringParams_);

    Matrix4x4 viewMatrix = camera ? camera->GetViewMatrix() : MakeIdentity4x4();
    Matrix4x4 projMatrix = camera ? camera->GetProjectionMatrix() : MakeIdentity4x4();
    ApplyTransform_(viewMatrix, projMatrix);
}

void RingEffect::ApplyTransform_(const Matrix4x4& viewMatrix, const Matrix4x4& projMatrix) {
    // ビルボード行列の計算
    Matrix4x4 billBoardMat = viewMatrix;
    billBoardMat.m[3][0] = 0.0f;
    billBoardMat.m[3][1] = 0.0f;
    billBoardMat.m[3][2] = 0.0f;
    billBoardMat = Inverse(billBoardMat); // カメラの逆回転

    // 平行移動とスケールを適用
    const Transform& transform = GetTransform();
    billBoardMat.m[3][0] = transform.translate.x;
    billBoardMat.m[3][1] = transform.translate.y;
    billBoardMat.m[3][2] = transform.translate.z;

    Matrix4x4 scaleMat = MakeScaleMatrix(transform.scale);
    Matrix4x4 worldMat = Multiply(scaleMat, billBoardMat);

    ring_.SetTransform(worldMat, viewMatrix, projMatrix);
    ring_.SetMaterial({1.0f, 1.0f, 1.0f, alpha_}, MakeIdentity4x4());
}

void RingEffect::Draw() {
    if (texture_) {
        // 描画の時点のカメラ(Renderer が持つ行列)で、向きと位置を計算し直す
        auto* renderer = Renderer::GetInstance();
        ApplyTransform_(renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
        renderer->DrawRing(&ring_, texture_->GetSrvGpu());
    }
}
