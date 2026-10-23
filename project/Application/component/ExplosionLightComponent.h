#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/LightNodeComponent.h"
#include <algorithm>

// 爆発の光。生成から lifeTime 秒かけて、光の強さを maxIntensity から 0 へ減らし、寿命が来たら消灯して IsDead() が true になる。
// 経過時間と終了の状態は内部だけで管理し、外からは生成時の設定と IsDead() の確認だけができる
class ExplosionLightComponent : public AbsoluteEngine::IComponent {
public:
    static constexpr float kDefaultLifeTime = 0.5f;
    static constexpr float kDefaultMaxIntensity = 5.0f;

    // lifeTime: 消えるまでの時間(秒)。maxIntensity: 生成直後の光の強さ
    explicit ExplosionLightComponent(float lifeTime = kDefaultLifeTime, float maxIntensity = kDefaultMaxIntensity)
        : lifeTime_(lifeTime), maxIntensity_(maxIntensity) {}

    void Update(float deltaTime) override {
        if (!owner_) return;

        timer_ += deltaTime;
        auto lightComp = owner_->GetComponent<AbsoluteEngine::LightNodeComponent>();
        if (lightComp) {
            if (timer_ >= lifeTime_) {
                isDead_ = true;
                lightComp->type = AbsoluteEngine::LightNodeComponent::Type::None;
            } else {
                float t = timer_ / lifeTime_;
                float currentIntensity = maxIntensity_ * (1.0f - t);
                lightComp->intensity = currentIntensity;
            }
        } else {
            if (timer_ >= lifeTime_) {
                isDead_ = true;
            }
        }
    }

    std::string GetTypeName() const override { return "ExplosionLightComponent"; }
    bool IsDead() const { return isDead_; }

private:
    float timer_ = 0.0f;
    float lifeTime_;
    float maxIntensity_;
    bool isDead_ = false;
};
