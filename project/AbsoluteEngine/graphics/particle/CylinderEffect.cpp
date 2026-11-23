#include "CylinderEffect.h"
#include "base/DirectXCommon.h"
#include "camera/Camera.h"
#include "graphics/Renderer.h"
#include "graphics/texture/TextureResource.h"
#include "math/Method.h"

CylinderEffect::CylinderEffect(std::shared_ptr<TextureResource> texture, const Vector3& position)
    : BaseEffect(kLifetime, position), texture_(std::move(texture)) {
    cylinderParams_.divide = 32;
    cylinderParams_.topRadiusX = 1.0f;
    cylinderParams_.topRadiusZ = 1.0f;
    cylinderParams_.bottomRadiusX = 1.0f;
    cylinderParams_.bottomRadiusZ = 1.0f;
    cylinderParams_.height = 0.1f;
    cylinderParams_.colorTop = {1.0f, 1.0f, 1.0f, 1.0f};
    cylinderParams_.colorBottom = {1.0f, 1.0f, 1.0f, 1.0f};
    
    cylinder_.Initialize(Renderer::GetInstance()->GetDX()->GetDevice(), cylinderParams_);
}

void CylinderEffect::OnUpdate(float t, float /*deltaTime*/, const Camera* camera) {
    // 時間経過で上に伸び、広がりながら透明になる
    cylinderParams_.height = maxHeight_ * t;
    float currentRadius = 1.0f + t * 0.5f;
    cylinderParams_.topRadiusX = currentRadius;
    cylinderParams_.topRadiusZ = currentRadius;
    cylinderParams_.bottomRadiusX = currentRadius;
    cylinderParams_.bottomRadiusZ = currentRadius;

    float alpha = 1.0f - t;
    cylinderParams_.colorTop.w = alpha;
    cylinderParams_.colorBottom.w = alpha;

    cylinder_.Update(Renderer::GetInstance()->GetDX()->GetDevice(), cylinderParams_);

    const Transform& transform = GetTransform();
    Matrix4x4 worldMat = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    Matrix4x4 viewMatrix = camera ? camera->GetViewMatrix() : MakeIdentity4x4();
    Matrix4x4 projMatrix = camera ? camera->GetProjectionMatrix() : MakeIdentity4x4();
    
    cylinder_.SetTransform(worldMat, viewMatrix, projMatrix);
    // テクスチャのUVスケール設定（縦方向の線を表現するためU方向にリピート）
    cylinder_.SetMaterial({1.0f, 1.0f, 1.0f, alpha}, MakeScaleMatrix({5.0f, 1.0f, 1.0f}));
}

void CylinderEffect::Draw() {
    if (texture_) {
        Renderer::GetInstance()->DrawCylinder(&cylinder_, texture_->GetSrvGpu());
    }
}
