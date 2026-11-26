#include "TelegraphComponent.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/scene/IScreenView.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "BodyTintGlow.h"
#include "EnemyComponent.h"
#include "EnemyShootUtil.h"
#include "MuzzleOrbGlow.h"
#include "externals/nlohmann/json.hpp"

namespace {
// JSON のキー名
constexpr const char* kKeyGlowMode = "glowMode";
constexpr const char* kKeyDuration = "duration";
constexpr const char* kKeyMuzzleOffset = "muzzleOffset";
constexpr const char* kKeyGlowColor = "glowColor";
constexpr const char* kKeyOrbScaleStart = "orbScaleStart";
constexpr const char* kKeyOrbScaleEnd = "orbScaleEnd";
constexpr const char* kKeyBodyGlowTint = "bodyGlowTint";
constexpr const char* kKeyShakeAmplitude = "shakeAmplitudeDeg";
constexpr const char* kKeyShakeFrequency = "shakeFrequency";
constexpr const char* kKeyScreenMargin = "screenMargin";
// glowMode の値
constexpr const char* kModeMuzzle = "muzzle";
constexpr const char* kModeBody = "body";

// 予備動作の長さを過ぎても、撃つ側が End を呼ばないとき(撃てなかったなど)に、光ったままにしない猶予(秒)
constexpr float kHoldAfterFullSeconds = 0.25f;
// 度からラジアンへ
constexpr float kPi = 3.14159265358979f;
constexpr float kHalfTurnDegrees = 180.0f;
constexpr float kTwoPi = 6.28318530717959f;

Vector3 ReadVector3(const nlohmann::json& j, const char* key, const Vector3& fallback) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_object()) return fallback;
    return { it->value("x", fallback.x), it->value("y", fallback.y), it->value("z", fallback.z) };
}

Vector4 ReadVector4(const nlohmann::json& j, const char* key, const Vector4& fallback) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_object()) return fallback;
    return { it->value("x", fallback.x), it->value("y", fallback.y), it->value("z", fallback.z), it->value("w", fallback.w) };
}
} // namespace

TelegraphComponent::TelegraphComponent(AbsoluteEngine::ISpawner& spawner, AbsoluteEngine::IScreenView& screen)
    : spawner_(spawner), screen_(screen) {
    RebuildGlow_();
}

// 光り方(glow_)の破棄で、光る玉も消える(MuzzleOrbGlow のデストラクタ)
TelegraphComponent::~TelegraphComponent() = default;

void TelegraphComponent::RebuildGlow_() {
    switch (glowMode_) {
    case GlowMode::Body:
        glow_ = std::make_unique<BodyTintGlow>(bodyGlowTint_);
        break;
    case GlowMode::Muzzle:
    default:
        glow_ = std::make_unique<MuzzleOrbGlow>(spawner_, MuzzleOrbGlow::Settings{ muzzleOffset_, glowColor_, orbScaleStart_, orbScaleEnd_ });
        break;
    }
}

float TelegraphComponent::GetProgress_() const {
    return std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
}

bool TelegraphComponent::IsAllowedToShow_() const {
    if (!GetOwner()) return false;
    // 画面外では出さない(撃っても、見えないため)
    if (!screen_.IsOnScreen(GetOwner()->GetTransform().translate, screenMargin_)) return false;
    // 島に隠れている間は出さない: 島の遮蔽の判定(EnemyShootUtil::IsHiddenByIsland)が、このブランチに入ったら、
    //   ここへ「if (EnemyShootUtil::IsHiddenByIsland(...)) return false;」を 1 行足す
    return true;
}

void TelegraphComponent::UpdateShake_() {
    if (!GetOwner() || shakeAmplitudeDeg_ <= 0.0f) return;
    const float angle = std::sin(elapsed_ * shakeFrequency_ * kTwoPi) * shakeAmplitudeDeg_ * kPi / kHalfTurnDegrees;
    SetOwnerRotateZ_(baseRotateZ_ + angle);
}

void TelegraphComponent::SetOwnerRotateZ_(float rotateZ) {
    if (!GetOwner()) return;
    // 回転は Z 成分だけを変えたい。現在の値をコピーして Z だけ差し替え、SetRotation で戻す
    Vector3 rotation = GetOwner()->GetTransform().rotate;
    rotation.z = rotateZ;
    GetOwner()->SetRotation(rotation);
}

void TelegraphComponent::Begin(float startElapsed) {
    if (playing_ || !GetOwner()) return;
    playing_ = true;
    elapsed_ = std::max(0.0f, startElapsed);
    baseRotateZ_ = GetOwner()->GetTransform().rotate.z;

    // 光り始める(光る玉を出す、体の色を替える、は光り方のクラスが決める)
    glow_->OnBegin(*GetOwner(), GetProgress_());
}

void TelegraphComponent::End() {
    if (!playing_) return;
    playing_ = false;

    // 見た目を元に戻す(光る玉を消す、体の色を戻す、は光り方のクラスが決める)
    glow_->OnEnd(GetOwner(), baseColor_);
    if (GetOwner()) {
        if (shakeAmplitudeDeg_ > 0.0f) {
            SetOwnerRotateZ_(baseRotateZ_);
        }
    }
}

bool TelegraphComponent::HoldFire(float secondsUntilFire) {
    if (!GetOwner()) return false;

    if (playing_) {
        // 再生中: 予備動作が済むまで待つ
        return elapsed_ < duration_;
    }
    // 撃つ時刻の duration 秒前になるまでは、何もしない(撃つ側の、普通の待ちに任せる)
    if (secondsUntilFire > duration_) return false;
    // 見せられないときは、予備動作なしで撃たせる
    if (!IsAllowedToShow_()) return false;

    // 始める。撃つ時刻に、ちょうど終わるように、途中から始める(撃つ時刻を過ぎていれば、最初から)
    const float startElapsed = (secondsUntilFire > 0.0f) ? (duration_ - secondsUntilFire) : 0.0f;
    Begin(startElapsed);
    return elapsed_ < duration_;
}

void TelegraphComponent::Update(float deltaTime) {
    if (!GetOwner()) return;

    // 元の体の色を、最初に覚える(被弾フラッシュの最中に覚えないよう、出現直後の Update で)
    if (!baseColorCaptured_) {
        if (const auto* model = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>()) {
            baseColor_ = model->GetColor();
            baseColorCaptured_ = true;
        }
    }
    if (!playing_) return;

    // 倒れた敵、見せられない状態(画面外、島の裏)になったら、すぐに終わる
    const auto* enemy = GetOwner()->GetComponent<EnemyComponent>();
    if ((enemy && enemy->IsDead()) || !IsAllowedToShow_()) {
        End();
        return;
    }

    elapsed_ += deltaTime;
    // 撃つ側が End を呼ばないまま、猶予を過ぎたら終わる(光りっぱなしにしない)
    if (elapsed_ >= duration_ + kHoldAfterFullSeconds) {
        End();
        return;
    }

    glow_->OnUpdate(*GetOwner(), GetProgress_());
    UpdateShake_();
}

void TelegraphComponent::Serialize(nlohmann::json& j) const {
    j[kKeyGlowMode] = (glowMode_ == GlowMode::Muzzle) ? kModeMuzzle : kModeBody;
    j[kKeyDuration] = duration_;
    j[kKeyMuzzleOffset] = { {"x", muzzleOffset_.x}, {"y", muzzleOffset_.y}, {"z", muzzleOffset_.z} };
    j[kKeyGlowColor] = { {"x", glowColor_.x}, {"y", glowColor_.y}, {"z", glowColor_.z}, {"w", glowColor_.w} };
    j[kKeyOrbScaleStart] = orbScaleStart_;
    j[kKeyOrbScaleEnd] = orbScaleEnd_;
    j[kKeyBodyGlowTint] = { {"x", bodyGlowTint_.x}, {"y", bodyGlowTint_.y}, {"z", bodyGlowTint_.z}, {"w", bodyGlowTint_.w} };
    j[kKeyShakeAmplitude] = shakeAmplitudeDeg_;
    j[kKeyShakeFrequency] = shakeFrequency_;
    j[kKeyScreenMargin] = screenMargin_;
}

void TelegraphComponent::Deserialize(const nlohmann::json& j) {
    // 予備動作の最中に設定が読み直されたとき(エディタの取り消しなど)は、いまの光り方で見た目を元に戻してから作り直す
    End();
    const std::string mode = j.value(kKeyGlowMode, std::string(kModeMuzzle));
    glowMode_ = (mode == kModeBody) ? GlowMode::Body : GlowMode::Muzzle;
    duration_ = std::max(0.01f, j.value(kKeyDuration, duration_));
    muzzleOffset_ = ReadVector3(j, kKeyMuzzleOffset, muzzleOffset_);
    glowColor_ = ReadVector4(j, kKeyGlowColor, glowColor_);
    orbScaleStart_ = j.value(kKeyOrbScaleStart, orbScaleStart_);
    orbScaleEnd_ = j.value(kKeyOrbScaleEnd, orbScaleEnd_);
    bodyGlowTint_ = ReadVector4(j, kKeyBodyGlowTint, bodyGlowTint_);
    shakeAmplitudeDeg_ = j.value(kKeyShakeAmplitude, shakeAmplitudeDeg_);
    shakeFrequency_ = j.value(kKeyShakeFrequency, shakeFrequency_);
    screenMargin_ = j.value(kKeyScreenMargin, screenMargin_);
    // 読んだ設定で、光り方を作り直す
    RebuildGlow_();
}
