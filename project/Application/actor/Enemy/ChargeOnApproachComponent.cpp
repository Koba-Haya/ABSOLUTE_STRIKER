#include "ChargeOnApproachComponent.h"
#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "AbsoluteEngine/scene/IScreenView.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "TelegraphComponent.h"
#include "externals/nlohmann/json.hpp"
#include <utility>

namespace {
// JSON のキー名
constexpr const char* kKeyTriggerRange = "triggerRange";
constexpr const char* kKeyChargeSpeed = "chargeSpeed";
constexpr const char* kKeyWindupPullSpeed = "windupPullSpeed";
}

Vector3 ChargeOnApproachComponent::EstimateCameraVelocity_(float deltaTime) {
    const Vector3 eye = screen_.GetEye();
    Vector3 cameraVelocity = { 0, 0, 0 };
    if (hasPrevEye_ && deltaTime > 0.0f) {
        cameraVelocity = {
            (eye.x - prevEye_.x) / deltaTime,
            (eye.y - prevEye_.y) / deltaTime,
            (eye.z - prevEye_.z) / deltaTime
        };
    }
    prevEye_ = eye;
    hasPrevEye_ = true;
    return cameraVelocity;
}

void ChargeOnApproachComponent::Update(float deltaTime) {
    if (!GetOwner()) return;

    // カメラを使う状態(接近待ち、予備動作、狙う)は、カメラが無いフレームは止まる。突進中は使わない
    Vector3 cameraVelocity = { 0, 0, 0 };
    if (state_->UsesCamera()) {
        if (!screen_.HasCamera()) return;
        cameraVelocity = EstimateCameraVelocity_(deltaTime);
    }

    const ChargeStateContext context{ *GetOwner(), query_, GetOwner()->GetComponent<TelegraphComponent>(), settings_, cameraVelocity, deltaTime };
    // 次の状態は、今の状態が決める。切り替えは Update のあと(実行中の状態を消さないため)。1 フレームに 1 回
    std::unique_ptr<IChargeState> next = state_->Update(context);
    if (next) {
        state_ = std::move(next);
        state_->Enter(context);
    }
}

void ChargeOnApproachComponent::Serialize(nlohmann::json& j) const {
    j[kKeyTriggerRange] = settings_.triggerRange;
    j[kKeyChargeSpeed] = settings_.chargeSpeed;
    j[kKeyWindupPullSpeed] = settings_.windupPullSpeed;
}

void ChargeOnApproachComponent::Deserialize(const nlohmann::json& j) {
    settings_.triggerRange = j.value(kKeyTriggerRange, settings_.triggerRange);
    settings_.chargeSpeed = j.value(kKeyChargeSpeed, settings_.chargeSpeed);
    settings_.windupPullSpeed = j.value(kKeyWindupPullSpeed, settings_.windupPullSpeed);
}
