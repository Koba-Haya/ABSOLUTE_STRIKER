#pragma once
#include "BaseEffect.h"
#include "graphics/3d/model/ModelInstance.h"

class PlaneHitEffect : public BaseEffect {
public:
    PlaneHitEffect(std::shared_ptr<ModelResource> modelRes, const Vector3& position);
    ~PlaneHitEffect() override = default;

    void Draw() override;

protected:
    // 進行度 progress に応じて、回転しながら大きくなり、透明になる(寿命の管理は BaseEffect::Update が行う)
    void OnUpdate(float progress, float deltaTime, const Camera* camera) override;

private:
    static constexpr float kLifetime = 0.5f;   // 寿命(秒)
    static constexpr float kSpinSpeed = 5.0f;  // 板が Z 軸まわりに回る速さ(ラジアン/秒)

    ModelInstance instance_;
    float spinAngle_ = 0.0f; // 板の Z 軸まわりの回転角(ラジアン)。この Effect だけが持つ状態
};
