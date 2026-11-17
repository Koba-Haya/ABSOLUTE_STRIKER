#include "IslandReleasePillar.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "Logger.h"
#include "Method.h"
#include "ModelResource.h"

namespace {
constexpr float kTwoPi = 6.28318530717959f;

float Saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// 始めは速く、終わりへ向けてゆるやかに止まる(1 - (1 - t)^3)
float EaseOutCubic(float t) {
  const float inv = 1.0f - Saturate(t);
  return 1.0f - inv * inv * inv;
}

// 0 〜 1 のなめらかな補間(始まりと終わりがゆるやか)
float Smooth01(float t) {
  t = Saturate(t);
  return t * t * (3.0f - 2.0f * t);
}
} // namespace

void IslandReleasePillar::Initialize() {
  using namespace IslandReleasePillarDefaults;
  ready_ = false;
  auto resource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(kModelPath);
  if (!resource) {
    Logger::GetInstance().Log("[IslandReleasePillar] failed to load the sphere model: %s", kModelPath);
    return;
  }
  auto initWithMode = [&resource](ModelInstance &model, int32_t mode) {
    ModelInstance::CreateInfo info{};
    info.resource = resource;
    info.lightingMode = mode;
    return model.Initialize(info);
  };
  ready_ = initWithMode(pillar_, ModelLightingMode::kSoftGlow);
  ready_ = initWithMode(ring_, ModelLightingMode::kRimGlow) && ready_;
  if (ready_) {
    pillar_.SetShininess(kPillarGlowPower);
    ring_.SetShininess(kRingSharpness);
    ring_.SetSpecularColor(kRingColor); // 縁の輪は、中心の色も同じにする(中心は、ほぼ透ける)
  }
}

void IslandReleasePillar::Begin(const Vector3 &base) {
  base_ = base;
  pillarVisible_ = false;
  ringVisible_ = false;
}

void IslandReleasePillar::Update(ClearPhase phase, float phaseProgress) {
  using namespace IslandReleasePillarDefaults;
  pillarVisible_ = false;
  ringVisible_ = false;
  if (!ready_ || phase != ClearPhase::Release) return;
  const float p = Saturate(phaseProgress);

  // 柱: 立ち上がり(伸びる)-> 脈打つ -> 終わりに薄れる。球を縦に引き伸ばし、中心を根元に置く(下半分は、島に埋まる)
  const float rise = kRiseShare > 0.0f ? EaseOutCubic(p / kRiseShare) : 1.0f;
  const float fade = kFadeShare > 0.0f ? 1.0f - Smooth01((p - (1.0f - kFadeShare)) / kFadeShare) : 1.0f;
  const float pulse = 1.0f + kPulseAmount * std::sin(p * kPulseCycles * kTwoPi);
  const float alpha = kPeakAlpha * fade * pulse;
  if (rise > kMinRiseRatio && alpha > 0.0f) {
    pillar_.SetWorld(MakeAffineMatrix(Vector3{kPillarRadius, kPillarHeight * rise, kPillarRadius}, Vector3{0.0f, 0.0f, 0.0f}, base_));
    pillar_.SetColor({kPillarColor.x, kPillarColor.y, kPillarColor.z, alpha});
    pillarVisible_ = true;
  }

  // 光の輪: 根元から広がりながら薄れる。球を縦に潰した、平たい形の縁
  const float ringT = kRingShare > 0.0f ? Saturate(p / kRingShare) : 1.0f;
  const float ringAlpha = kRingPeakAlpha * (1.0f - ringT) * (1.0f - ringT);
  if (ringT < 1.0f && ringAlpha > 0.0f) {
    const float radius = Lerp(kRingStartRadius, kRingEndRadius, EaseOutCubic(ringT));
    ring_.SetWorld(MakeAffineMatrix(Vector3{radius, kRingThickness, radius}, Vector3{0.0f, 0.0f, 0.0f}, base_));
    ring_.SetColor({kRingColor.x, kRingColor.y, kRingColor.z, ringAlpha});
    ringVisible_ = true;
  }
}

void IslandReleasePillar::Draw(Renderer *renderer) {
  if (!ready_ || !renderer) return;
  // アルファ合成のエフェクト用パイプライン(深度は書かない。島や木より奥の部分は、深度テストで隠れる)
  if (pillarVisible_) renderer->DrawEffectModel(&pillar_, false);
  if (ringVisible_) renderer->DrawEffectModel(&ring_, false);
}
