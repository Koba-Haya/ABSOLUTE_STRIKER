#pragma once
#include "BaseEffect.h"
#include "graphics/primitive/Cylinder.h"
#include <memory>

class TextureResource;

class CylinderEffect : public BaseEffect {
public:
    // texture は shared_ptr で受け取る（RingEffectと同じ理由: EffectManagerはシーンをまたいで
    // 生存するシングルトンのため、呼び出し元が破棄された後もこのEffectが生きている間は
    // テクスチャが解放されないようにする）。デバイスは内部でRenderer::GetInstance()から取得する
    CylinderEffect(std::shared_ptr<TextureResource> texture, const Vector3& position);
    ~CylinderEffect() override = default;

    void Draw() override;

protected:
    // 進行度 progress に応じて、上へ伸びて広がり、透明になる(寿命の管理は BaseEffect::Update が行う)
    void OnUpdate(float progress, float deltaTime, const Camera* camera) override;

private:
    static constexpr float kLifetime = 0.8f; // 寿命(秒)

    Cylinder cylinder_;
    Cylinder::Params cylinderParams_{};
    std::shared_ptr<TextureResource> texture_;
    float maxHeight_ = 8.0f;
};
