#include "RailAnchorComponent.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../../camera/RailCameraComponent.h"
#include "Method.h"

namespace {
// JSON のキー名(保存・読み込みで同じ名前を使うため、定数にまとめる)
constexpr const char* kKeyAheadSpeed = "aheadSpeed";
constexpr const char* kKeyLateralSpeed = "lateralSpeed";
constexpr const char* kKeyVerticalSpeed = "verticalSpeed";
constexpr const char* kKeyMirror = "mirrorWithStartSide";
constexpr const char* kKeyLifetime = "lifetime";
} // namespace

RailCameraComponent* RailAnchorComponent::FindRailCamera_() const {
    auto* scene = BaseScene::GetActiveScene();
    if (!scene) return nullptr;
    for (const auto& obj : scene->GetRootObjects()) {
        if (!obj) continue;
        if (auto* camera = obj->GetComponent<RailCameraComponent>()) {
            return camera;
        }
    }
    return nullptr;
}

bool RailAnchorComponent::GetCameraDistance_(float& outDistance) const {
    if (!railCamera_) return false;
    // 進行度(0-1)を距離(ユニット)に直す。終点で止まっている(ボス戦)ときも、1.0 x 全長で正しく動く
    outDistance = railCamera_->ProgressToDistance(railCamera_->GetProgress());
    return true;
}

void RailAnchorComponent::OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) {
    if (!owner_) return;

    railCamera_ = FindRailCamera_();
    float cameraDistance = 0.0f;
    if (!GetCameraDistance_(cameraDistance)) {
        // レールカメラが見つからないときは結びつけない(その場に出現したまま、動かない)
        anchored_ = false;
        return;
    }

    // 出現位置を、出現したフレームの座標系に直す。
    //   ahead   = 出現した距離 - カメラの今の距離(カメラの1フレーム分のずれがあっても、位置は連続する)
    //   lateral = 出現位置が、フレームの右方向にどれだけずれているか(編隊の散らしがあっても正しく求まる)
    //   vertical= 同じく、上方向にどれだけずれているか
    const Vector3 spawnPos = owner_->GetTransform().translate;
    const Vector3 offset = { spawnPos.x - frame.position.x, spawnPos.y - frame.position.y, spawnPos.z - frame.position.z };
    aheadOffset_ = frame.distance - cameraDistance;
    lateral_ = Dot(offset, frame.right);
    vertical_ = Dot(offset, frame.up);

    // 横の動きの向き。左(lateral < 0)で出れば右へ、右で出れば左へ、ちょうど中央なら右へ
    lateralDirection_ = 1.0f;
    if (mirrorWithStartSide_ && lateral_ > 0.0f) {
        lateralDirection_ = -1.0f;
    }

    elapsed_ = 0.0f;
    anchored_ = true;
}

void RailAnchorComponent::Update(float deltaTime) {
    if (!owner_ || !anchored_) return;

    // 寿命が来たら、自分で消える(画面を横切り終えた敵の後始末)
    elapsed_ += deltaTime;
    if (lifetime_ > 0.0f && elapsed_ >= lifetime_) {
        owner_->Destroy();
        return;
    }

    float cameraDistance = 0.0f;
    if (!GetCameraDistance_(cameraDistance)) return;

    // レール座標を、速さぶん進める
    aheadOffset_ += aheadSpeed_ * deltaTime;
    lateral_ += lateralSpeed_ * lateralDirection_ * deltaTime;
    vertical_ += verticalSpeed_ * deltaTime;

    // レール座標から世界座標へ。カメラの距離 + ahead の地点の座標系を取り、右・上へずらす(曲がったレールでも道に沿う)
    const AbsoluteEngine::RailFrame frame = railCamera_->GetRailFrame(cameraDistance + aheadOffset_);
    auto& t = owner_->GetTransform();
    t.translate = {
        frame.position.x + frame.right.x * lateral_ + frame.up.x * vertical_,
        frame.position.y + frame.right.y * lateral_ + frame.up.y * vertical_,
        frame.position.z + frame.right.z * lateral_ + frame.up.z * vertical_ };
}

void RailAnchorComponent::Serialize(nlohmann::json& j) const {
    j[kKeyAheadSpeed] = aheadSpeed_;
    j[kKeyLateralSpeed] = lateralSpeed_;
    j[kKeyVerticalSpeed] = verticalSpeed_;
    j[kKeyMirror] = mirrorWithStartSide_;
    j[kKeyLifetime] = lifetime_;
}

void RailAnchorComponent::Deserialize(const nlohmann::json& j) {
    aheadSpeed_ = j.value(kKeyAheadSpeed, aheadSpeed_);
    lateralSpeed_ = j.value(kKeyLateralSpeed, lateralSpeed_);
    verticalSpeed_ = j.value(kKeyVerticalSpeed, verticalSpeed_);
    mirrorWithStartSide_ = j.value(kKeyMirror, mirrorWithStartSide_);
    lifetime_ = j.value(kKeyLifetime, lifetime_);
}
