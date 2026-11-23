#include "PlaneHitEffect.h"
#include "camera/Camera.h"
#include "graphics/Renderer.h"
#include "math/Method.h"

PlaneHitEffect::PlaneHitEffect(std::shared_ptr<ModelResource> modelRes, const Vector3& position)
    : BaseEffect(kLifetime, position) {
    instance_.Initialize({modelRes, {1.0f, 1.0f, 1.0f, 1.0f}, 0});
}

void PlaneHitEffect::OnUpdate(float t, float deltaTime, const Camera* camera) {
    float scaleVal = t * 5.0f;
    Vector3 currentScale = {scaleVal, scaleVal, scaleVal};
    
    Matrix4x4 viewMatrix = camera ? camera->GetViewMatrix() : MakeIdentity4x4();
    Matrix4x4 billBoardMat = viewMatrix;
    billBoardMat.m[3][0] = 0.0f;
    billBoardMat.m[3][1] = 0.0f;
    billBoardMat.m[3][2] = 0.0f;
    billBoardMat = Inverse(billBoardMat); 

    spinAngle_ += kSpinSpeed * deltaTime;
    Matrix4x4 rotZ = MakeRotateZMatrix(spinAngle_);
    
    Matrix4x4 rotMat = Multiply(rotZ, billBoardMat);
    Matrix4x4 scaleMat = MakeScaleMatrix(currentScale);
    
    Matrix4x4 worldMat = Multiply(scaleMat, rotMat);
    const Vector3& position = GetTransform().translate;
    worldMat.m[3][0] = position.x;
    worldMat.m[3][1] = position.y;
    worldMat.m[3][2] = position.z;

    instance_.SetWorld(worldMat);
    instance_.SetColor({1.0f, 1.0f, 1.0f, 1.0f - t});
}

void PlaneHitEffect::Draw() {
    Renderer::GetInstance()->DrawEffectModel(&instance_);
}
