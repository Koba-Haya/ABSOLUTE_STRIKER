#include "ItemPickupComponent.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Player/PlayerComponent.h"

namespace {
// 取得の演出のうち、「膨らむ」段階の割合(残りは、縮みながら吸い込まれる)
constexpr float kCollectPeakRatio = 0.3f;
// 取得の演出中の、自機へ向かう速さの倍率(吸い寄せの速さに対する)
constexpr float kCollectPullSpeedScale = 2.0f;
// 吸い寄せの加速の強さ。自機に近いほど速くなる(0 なら一定。1 なら、最も近いとき 2 倍)
constexpr float kPullAccelerationBoost = 1.0f;

float Distance(const Vector3 &a, const Vector3 &b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}
} // namespace

void ItemPickupComponent::Configure(const ItemPickupSettings &settings, float baseScale, std::function<void()> onCollected) {
  settings_ = settings;
  baseScale_ = baseScale;
  onCollected_ = std::move(onCollected);
}

std::shared_ptr<AbsoluteEngine::GameObject> ItemPickupComponent::FindPlayer_() {
  if (auto cached = player_.lock()) return cached;

  BaseScene *scene = BaseScene::GetActiveScene();
  if (!scene) return nullptr;
  for (const auto &obj : scene->GetRootObjects()) {
    if (obj && obj->GetComponent<PlayerComponent>()) {
      player_ = obj;
      return obj;
    }
  }
  return nullptr;
}

void ItemPickupComponent::MoveToward_(const Vector3 &target, float speed, float deltaTime) {
  const float distance = Distance(basePosition_, target);
  if (distance < 0.0001f) return;
  // 1 フレームで行き過ぎない(目標を越えない)ようにする
  const float step = std::min(speed * deltaTime, distance);
  const float ratio = step / distance;
  basePosition_.x += (target.x - basePosition_.x) * ratio;
  basePosition_.y += (target.y - basePosition_.y) * ratio;
  basePosition_.z += (target.z - basePosition_.z) * ratio;
}

void ItemPickupComponent::ApplyVisual_(float scale) {
  auto &transform = owner_->GetTransform();
  transform.translate = {basePosition_.x, basePosition_.y + std::sin(age_ * settings_.bobSpeed) * settings_.bobAmplitude,
                         basePosition_.z};
  transform.rotate.y = spinAngle_;
  transform.scale = {scale, scale, scale};
}

void ItemPickupComponent::Update(float deltaTime) {
  if (!owner_) return;

  // ゆれを除いた位置は、最初の Update で、出現した位置から決める
  if (!initialized_) {
    basePosition_ = owner_->GetTransform().translate;
    initialized_ = true;
  }
  age_ += deltaTime;
  spinAngle_ += settings_.spinSpeed * deltaTime;

  const auto playerObj = FindPlayer_();

  // 取得の演出中: 少し膨らんでから縮みつつ、自機へ吸い込まれて、終わったら消える
  if (collected_) {
    collectTimer_ += deltaTime;
    const float t = std::clamp(collectTimer_ / settings_.collectSeconds, 0.0f, 1.0f);
    if (t >= 1.0f) {
      owner_->Destroy();
      return;
    }
    if (playerObj) {
      MoveToward_(playerObj->GetTransform().translate, settings_.pullSpeed * kCollectPullSpeedScale, deltaTime);
    }
    const float factor = (t < kCollectPeakRatio)
                             ? 1.0f + (settings_.collectScaleUp - 1.0f) * (t / kCollectPeakRatio)
                             : settings_.collectScaleUp * (1.0f - (t - kCollectPeakRatio) / (1.0f - kCollectPeakRatio));
    ApplyVisual_(baseScale_ * factor);
    return;
  }

  // 取られなかったら、寿命で消える
  if (age_ >= settings_.lifetimeSeconds) {
    owner_->Destroy();
    return;
  }

  // 自機が死亡演出中のときは、吸い寄せも取得もしない(死亡演出の最中に効果が出ないように)
  const auto *playerComp = playerObj ? playerObj->GetComponent<PlayerComponent>() : nullptr;
  if (playerObj && playerComp && !playerComp->IsDying() && playerComp->GetHp() > 0) {
    const Vector3 playerPos = playerObj->GetTransform().translate;
    const float distance = Distance(basePosition_, playerPos);
    if (distance <= settings_.pickupRadius) {
      // 取得: 効果は、この瞬間に 1 回だけ出す
      collected_ = true;
      collectTimer_ = 0.0f;
      if (onCollected_) {
        onCollected_();
      }
    } else if (distance <= settings_.pullRadius) {
      // 吸い寄せ: 近いほど速くする(離れ際に追いつけず、取り逃さないように)
      const float closeness = 1.0f - distance / settings_.pullRadius;
      MoveToward_(playerPos, settings_.pullSpeed * (1.0f + kPullAccelerationBoost * closeness), deltaTime);
    }
  }

  ApplyVisual_(baseScale_);
}
