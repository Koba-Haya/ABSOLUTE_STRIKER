#include "BossExplosion.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "Logger.h"
#include "Method.h"
#include "ModelResource.h"

namespace {
float Saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

// 始めは速く、終わりへ向けてゆるやかに止まる(1 - (1 - t)^3)
float EaseOutCubic(float t) {
  const float inv = 1.0f - Saturate(t);
  return 1.0f - inv * inv * inv;
}

float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}
} // namespace

void BossExplosion::Initialize(const char *modelPath) {
  ready_ = false;
  auto resource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(modelPath);
  if (!resource) {
    Logger::GetInstance().Log("[BossExplosion] failed to load the sphere model: %s", modelPath);
    return;
  }
  ModelInstance::CreateInfo info{};
  info.resource = resource;
  info.lightingMode = ModelLightingMode::kRimGlow;
  ready_ = model_.Initialize(info);
}

void BossExplosion::Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, const BossCrackShape &crackShape,
                          const BossExplosionSettings &settings) {
  shell_ = std::move(shell);
  crackShape_ = crackShape;
  settings_ = settings;
  alpha_ = 0.0f;
  // 球の見た目(縁の色と中心の色、縁の鋭さ)は、演出の間、変わらない。濃さと大きさだけを、毎フレーム変える
  if (ready_) {
    model_.SetSpecularColor(settings_.centerColor);
    model_.SetShininess(settings_.rimSharpness);
  }
}

void BossExplosion::Update(ClearPhase phase, float phaseProgress) {
  alpha_ = 0.0f;
  if (!ready_ || !shell_) return;
  // ひび割れの形がない(従来のディゾルブで消える)ボスは、球を出さない(中心が決まらないため)
  if (crackShape_.cellSize <= 0.0f) return;

  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  if (!modelComponent) return;

  const float p = Saturate(phaseProgress);
  float radius = 0.0f;
  float alpha = 0.0f;
  if (phase == ClearPhase::Peel) {
    // 崩壊と同時に、中心から広がる。濃さは、始めに立ち上がる(いきなり出ない)
    radius = Lerp(settings_.startRadius, settings_.maxRadius, EaseOutCubic(p));
    alpha = settings_.peakAlpha * (settings_.rampShare > 0.0f ? Saturate(p / settings_.rampShare) : 1.0f);
  } else if (phase == ClearPhase::Burst) {
    // さらに少し広がりながら、薄れて消える
    radius = settings_.maxRadius * Lerp(1.0f, settings_.burstGrow, p);
    const float remain = 1.0f - p;
    alpha = settings_.peakAlpha * remain * remain;
  } else {
    return;
  }
  if (alpha <= 0.0f) return;

  // 球の中心: ひびの広がり始める位置(体の中心)。体の動きに付いてくる
  const Vector3 center = TransformPoint({crackShape_.originX, crackShape_.originY, crackShape_.originZ}, modelComponent->GetWorldMatrix());
  // 回転は使わない(球は、どの向きでも同じ)。Vector3 と明示するのは、回転の引数に、同じ形の別の型(Quaternion)が選ばれる曖昧さを避けるため
  model_.SetWorld(MakeAffineMatrix(Vector3{radius, radius, radius}, Vector3{0.0f, 0.0f, 0.0f}, center));
  model_.SetColor({settings_.rimColor.x, settings_.rimColor.y, settings_.rimColor.z, alpha});
  alpha_ = alpha;
}

void BossExplosion::Draw(Renderer *renderer) {
  if (!ready_ || alpha_ <= 0.0f || !renderer) return;
  // 半透明のエフェクト用パイプライン(アルファ合成、深度は書かない)で描く。球の中の物が透けて見える
  renderer->DrawEffectModel(&model_, false);
}
