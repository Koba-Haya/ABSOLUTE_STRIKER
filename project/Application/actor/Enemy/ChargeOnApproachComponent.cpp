#include "ChargeOnApproachComponent.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../../actor/Player/PlayerComponent.h"
#include "GameCamera.h"
#include "TelegraphComponent.h"
#include "externals/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>

namespace {
// JSON のキー名
constexpr const char* kKeyTriggerRange = "triggerRange";
constexpr const char* kKeyChargeSpeed = "chargeSpeed";
constexpr const char* kKeyWindupPullSpeed = "windupPullSpeed";
}

void ChargeOnApproachComponent::Update(float deltaTime) {
    if (!owner_) return;

    auto& t = owner_->GetTransform();

    if (triggered_) {
        // 突進中: 発火瞬間に確定した方向へ直進し続ける（毎フレーム再計算しない）
        t.translate.x += chargeDir_.x * chargeSpeed_ * deltaTime;
        t.translate.y += chargeDir_.y * chargeSpeed_ * deltaTime;
        t.translate.z += chargeDir_.z * chargeSpeed_ * deltaTime;
        return;
    }

    auto scene = BaseScene::GetActiveScene();
    if (!scene) return;
    auto* camera = scene->GetMainCamera();
    if (!camera) return;

    // カメラの移動速度を推定する（前フレームとのeye差分）。
    // プレイヤーはeye基準で毎フレーム位置決めされるため、操作していなくても
    // カメラのレール前進と同じ速度でワールド座標が流れ続ける。
    const Vector3 eye = camera->GetEye();
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

    // --- 予備動作の最中: 自機と反対へ引く(だんだん遅くなる)。済んだら、次のフレームで突進を始める ---
    auto* telegraph = owner_->GetComponent<TelegraphComponent>();
    if (windingUp_) {
        windupTimer_ += deltaTime;
        const float duration = telegraph ? telegraph->GetDuration() : 0.0f;
        const float progress = (duration > 0.0f) ? std::min(windupTimer_ / duration, 1.0f) : 1.0f;
        const float pull = windupPullSpeed_ * (1.0f - progress) * deltaTime;
        t.translate.x += windupAwayDir_.x * pull;
        t.translate.y += windupAwayDir_.y * pull;
        t.translate.z += windupAwayDir_.z * pull;
        if (progress >= 1.0f) {
            windingUp_ = false;
            windupDone_ = true;
            if (telegraph) telegraph->End();
        }
        return;
    }

    // --- プレイヤーを探し、距離判定 ---
    std::shared_ptr<AbsoluteEngine::GameObject> playerObj = nullptr;
    for (const auto& obj : scene->GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            playerObj = obj;
            break;
        }
    }
    if (!playerObj) return;

    const Vector3 currentPos = t.translate;
    const Vector3 playerPos = playerObj->GetTransform().translate;
    Vector3 toPlayer = {
        playerPos.x - currentPos.x,
        playerPos.y - currentPos.y,
        playerPos.z - currentPos.z
    };
    const float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    // 予備動作が済んでいれば、引いて距離が開いていても、そのまま突進を始める
    if (!windupDone_ && (dist > triggerRange_ || dist < 0.001f)) return;
    if (dist < 0.001f) return;

    // --- 予備動作を始める(TelegraphComponent があるときだけ) ---
    if (!windupDone_ && telegraph) {
        windingUp_ = true;
        windupTimer_ = 0.0f;
        windupAwayDir_ = { -toPlayer.x / dist, -toPlayer.y / dist, -toPlayer.z / dist };
        telegraph->Begin();
        return;
    }

    // --- トリガー発火 ---
    // 到達までのおおよその時間を見積もり、カメラの推定移動速度ぶんだけ
    // 「到達する頃にいるはずの位置」を先読みして、方向を一度だけ確定する。
    // （以後は毎フレーム再計算しない＝プレイヤーの回避操作には追従しない）
    triggered_ = true;

    const float timeToImpact = (chargeSpeed_ > 0.001f) ? (dist / chargeSpeed_) : 0.0f;
    const Vector3 predictedPos = {
        playerPos.x + cameraVelocity.x * timeToImpact,
        playerPos.y + cameraVelocity.y * timeToImpact,
        playerPos.z + cameraVelocity.z * timeToImpact
    };
    Vector3 toPredicted = {
        predictedPos.x - currentPos.x,
        predictedPos.y - currentPos.y,
        predictedPos.z - currentPos.z
    };
    const float predDist = std::sqrt(toPredicted.x * toPredicted.x + toPredicted.y * toPredicted.y + toPredicted.z * toPredicted.z);
    if (predDist > 0.001f) {
        chargeDir_ = { toPredicted.x / predDist, toPredicted.y / predDist, toPredicted.z / predDist };
    } else {
        chargeDir_ = { toPlayer.x / dist, toPlayer.y / dist, toPlayer.z / dist };
    }
}

void ChargeOnApproachComponent::Serialize(nlohmann::json& j) const {
    j[kKeyTriggerRange] = triggerRange_;
    j[kKeyChargeSpeed] = chargeSpeed_;
    j[kKeyWindupPullSpeed] = windupPullSpeed_;
}

void ChargeOnApproachComponent::Deserialize(const nlohmann::json& j) {
    triggerRange_ = j.value(kKeyTriggerRange, triggerRange_);
    chargeSpeed_ = j.value(kKeyChargeSpeed, chargeSpeed_);
    windupPullSpeed_ = j.value(kKeyWindupPullSpeed, windupPullSpeed_);
}
