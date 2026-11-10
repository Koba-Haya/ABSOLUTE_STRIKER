#include "TelegraphComponent.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "EnemyComponent.h"
#include "EnemyShootUtil.h"
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

// 光る玉のモデルとテクスチャ(exe 基準)。球は半径 1
constexpr const char* kOrbModelPath = "resources/app/sphere/sphere.obj";
constexpr const char* kOrbTexturePath = "resources/app/cube/white100x100.png";
constexpr const char* kOrbObjectName = "TelegraphGlow";
constexpr const char* kOrbTag = "TelegraphGlow";

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

TelegraphComponent::~TelegraphComponent() {
    DestroyOrb_();
}

bool TelegraphComponent::IsAllowedToShow_() const {
    if (!owner_) return false;
    const BaseScene* scene = BaseScene::GetActiveScene();
    if (!scene) return false;
    // 画面外では出さない(撃っても、見えないため)
    if (!EnemyShootUtil::IsInsideScreen(*scene, owner_->GetTransform().translate, screenMargin_)) return false;
    // 島に隠れている間は出さない: 島の遮蔽の判定(EnemyShootUtil::IsHiddenByIsland)が、このブランチに入ったら、
    //   ここへ「if (EnemyShootUtil::IsHiddenByIsland(...)) return false;」を 1 行足す
    return true;
}

void TelegraphComponent::CreateOrb_() {
    if (orb_.lock()) return;
    BaseScene* scene = BaseScene::GetActiveScene();
    if (!scene) return;

    auto orb = std::make_shared<AbsoluteEngine::GameObject>(kOrbObjectName);
    orb->SetTag(kOrbTag);
    auto model = std::make_unique<AbsoluteEngine::ModelComponent>();
    model->SetLightingMode(ModelLightingMode::kUnlit); // 照明なし(光って見える)
    model->LoadModel(kOrbModelPath);
    model->LoadTexture(kOrbTexturePath);
    model->SetColor(glowColor_);
    orb->AddComponent(std::move(model));
    scene->AddRootObject(orb);
    orb_ = orb;
}

void TelegraphComponent::DestroyOrb_() {
    if (auto orb = orb_.lock()) {
        orb->Destroy();
    }
    orb_.reset();
}

void TelegraphComponent::UpdateOrb_(float progress) {
    auto orb = orb_.lock();
    if (!orb || !owner_) return;

    const auto& ownerTransform = owner_->GetTransform();
    const Vector3& s = ownerTransform.scale;
    // 出口の位置: 敵の位置 + 出口のずれ x 敵の拡大率(敵の回転は見ない。今の敵は、回転せずに出る)
    orb->GetTransform().translate = { ownerTransform.translate.x + muzzleOffset_.x * s.x,
                                      ownerTransform.translate.y + muzzleOffset_.y * s.y,
                                      ownerTransform.translate.z + muzzleOffset_.z * s.z };
    // 大きさ: 始めから終わりへ、なめらかに(終わりに近づくほど速く膨らむ)。敵の拡大率の平均を掛ける
    const float eased = progress * progress;
    const float orbScale = (orbScaleStart_ + (orbScaleEnd_ - orbScaleStart_) * eased) * ((s.x + s.y + s.z) / 3.0f);
    orb->GetTransform().scale = { orbScale, orbScale, orbScale };
}

void TelegraphComponent::UpdateShake_() {
    if (!owner_ || shakeAmplitudeDeg_ <= 0.0f) return;
    const float angle = std::sin(elapsed_ * shakeFrequency_ * kTwoPi) * shakeAmplitudeDeg_ * kPi / kHalfTurnDegrees;
    owner_->GetTransform().rotate.z = baseRotateZ_ + angle;
}

void TelegraphComponent::Begin(float startElapsed) {
    if (playing_ || !owner_) return;
    playing_ = true;
    elapsed_ = std::max(0.0f, startElapsed);
    baseRotateZ_ = owner_->GetTransform().rotate.z;

    if (glowMode_ == GlowMode::Muzzle) {
        CreateOrb_();
        UpdateOrb_(std::clamp(elapsed_ / duration_, 0.0f, 1.0f));
    } else if (auto* model = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
        model->SetColor(bodyGlowTint_);
    }
}

void TelegraphComponent::End() {
    if (!playing_) return;
    playing_ = false;

    DestroyOrb_();
    if (owner_) {
        if (glowMode_ == GlowMode::Body) {
            if (auto* model = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
                model->SetColor(baseColor_);
            }
        }
        if (shakeAmplitudeDeg_ > 0.0f) {
            owner_->GetTransform().rotate.z = baseRotateZ_;
        }
    }
}

bool TelegraphComponent::HoldFire(float secondsUntilFire) {
    if (!owner_) return false;

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
    if (!owner_) return;

    // 元の体の色を、最初に覚える(被弾フラッシュの最中に覚えないよう、出現直後の Update で)
    if (!baseColorCaptured_) {
        if (const auto* model = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
            baseColor_ = model->GetColor();
            baseColorCaptured_ = true;
        }
    }
    if (!playing_) return;

    // 倒れた敵、見せられない状態(画面外、島の裏)になったら、すぐに終わる
    const auto* enemy = owner_->GetComponent<EnemyComponent>();
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

    const float progress = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
    if (glowMode_ == GlowMode::Muzzle) {
        UpdateOrb_(progress);
    }
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
}
