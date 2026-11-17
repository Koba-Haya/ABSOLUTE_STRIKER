#include "BossExplosion.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "Logger.h"
#include "Method.h"
#include "ModelResource.h"
#include "StageImpactEffects.h"

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

// 衝撃波のリングの、広がる時間の割合の下限(リングの数が多すぎて、0 以下になるのを防ぐ)
constexpr float kMinShockwaveSpan = 0.2f;
// 核が深度を書く濃さの下限。この濃さ以上のあいだは、核の後ろ(球の中、裏側)にある粒を隠す。
// 爆散で核が薄れたあとは、深度を書かない(粒が、薄れていく核を透かして見え、解き放たれた様子が見える)
constexpr float kCoreDepthMinAlpha = 0.5f;
// ハロの濃さの、Peel の始まりの割合(核が小さいうちは、光のにじみも弱く、Peel の終わりへ向けて、最大まで強まる)
constexpr float kHaloStartShare = 0.5f;
} // namespace

void BossExplosion::PlaceSphere_(ModelInstance &model, const Vector3 &center, float radius, const Vector3 &color, float alpha) {
  // 回転は使わない(球は、どの向きでも同じ)。Vector3 と明示するのは、回転の引数に、同じ形の別の型(Quaternion)が選ばれる曖昧さを避けるため
  model.SetWorld(MakeAffineMatrix(Vector3{radius, radius, radius}, Vector3{0.0f, 0.0f, 0.0f}, center));
  model.SetColor({color.x, color.y, color.z, alpha});
}

void BossExplosion::Initialize(const char *modelPath) {
  ready_ = false;
  auto resource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(modelPath);
  if (!resource) {
    Logger::GetInstance().Log("[BossExplosion] failed to load the sphere model: %s", modelPath);
    return;
  }
  // 同じ球のモデルを、モードを変えて使う(核 = プラズマ球、ハロ = 光の輪、リング = 縁の鋭い球)
  auto initWithMode = [&resource](ModelInstance &model, int32_t mode) {
    ModelInstance::CreateInfo info{};
    info.resource = resource;
    info.lightingMode = mode;
    return model.Initialize(info);
  };
  ready_ = initWithMode(core_, ModelLightingMode::kPlasma);
  ready_ = initWithMode(halo_, ModelLightingMode::kSoftGlow) && ready_;
  for (auto &ring : shockwaves_) {
    ready_ = initWithMode(ring, ModelLightingMode::kRimGlow) && ready_;
  }
  // 爆散のフラッシュ(読めなければ、フラッシュが出ないだけ)
  flash_.Initialize(BossExplosionDefaults::kFlashModelPath);
}

void BossExplosion::Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, const BossCrackShape &crackShape,
                          const BossExplosionSettings &settings) {
  shell_ = std::move(shell);
  crackShape_ = crackShape;
  settings_ = settings;
  coreAlpha_ = 0.0f;
  haloAlpha_ = 0.0f;
  shockwaveAlpha_.fill(0.0f);
  flashFired_ = false;
  phase_ = 0.0f;
  // 変わらない見た目(縁の鋭さ、ハロの落ち方、リングの細さと中心の色)は、ここで 1 回だけ設定する。濃さと大きさは、毎フレーム変える
  if (ready_) {
    core_.SetShininess(settings_.rimPower);
    halo_.SetShininess(settings_.haloPower);
    for (auto &ring : shockwaves_) {
      ring.SetSpecularColor(settings_.shockwaveColor);
      ring.SetShininess(settings_.shockwaveSharpness);
    }
  }
}

void BossExplosion::Update(ClearPhase phase, float phaseProgress, float deltaTime) {
  coreAlpha_ = 0.0f;
  haloAlpha_ = 0.0f;
  shockwaveAlpha_.fill(0.0f);
  flash_.Update(deltaTime);
  if (!ready_ || !shell_) return;
  // ひび割れの形がない(従来のディゾルブで消える)ボスは、球を出さない(中心が決まらないため)
  if (crackShape_.cellSize <= 0.0f) return;

  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  if (!modelComponent) return;

  phase_ += deltaTime;
  const float p = Saturate(phaseProgress);
  float radius = 0.0f;
  float coreAlpha = 0.0f;
  float haloAlpha = 0.0f;
  float patch = 0.0f;
  float haloGrow = 1.0f; // ハロの半径の、さらなる広がり(Burst で、外へ広がって消える)
  if (phase == ClearPhase::Peel) {
    // 崩壊と同時に、中心から広がる。濃さは、始めに立ち上がる(いきなり出ない)。斑は、薄れて、純白になる
    radius = Lerp(settings_.startRadius, settings_.maxRadius, EaseOutCubic(p));
    coreAlpha = settings_.rampShare > 0.0f ? Saturate(p / settings_.rampShare) : 1.0f;
    patch = settings_.patchStart * (1.0f - p);
    haloAlpha = settings_.haloAlpha * coreAlpha * Lerp(kHaloStartShare, 1.0f, p);
  } else if (phase == ClearPhase::Burst) {
    // さらに少し広がりながら、核は、すばやく薄れる。ハロは、外へ広がりながら消える
    radius = settings_.maxRadius * Lerp(1.0f, settings_.burstGrow, EaseOutCubic(p));
    const float remain = 1.0f - p;
    coreAlpha = remain * remain;
    haloAlpha = settings_.haloAlpha * remain;
    haloGrow = Lerp(1.0f, settings_.burstGrow, p);
    // 爆散の瞬間(Burst の最初)に、白いフラッシュを 1 回だけ出す
    if (!flashFired_) {
      flash_.Trigger(settings_.flash);
      flashFired_ = true;
    }
  } else {
    return;
  }

  // 球の中心: ひびの広がり始める位置(体の中心)。体の動きに付いてくる
  const Vector3 center = TransformPoint({crackShape_.originX, crackShape_.originY, crackShape_.originZ}, modelComponent->GetWorldMatrix());
  if (coreAlpha > 0.0f) {
    core_.SetEnvironmentCoefficient(patch);
    // 斑が流れる位相は、プラズマ球の SetSpecularColor の x で渡す(ModelLightingMode::kPlasma の取り決め)
    core_.SetSpecularColor({phase_, 0.0f, 0.0f});
    PlaceSphere_(core_, center, radius, settings_.rimColor, coreAlpha);
    coreAlpha_ = coreAlpha;
  }
  if (haloAlpha > 0.0f) {
    PlaceSphere_(halo_, center, radius * settings_.haloScale * haloGrow, settings_.haloColor, haloAlpha);
    haloAlpha_ = haloAlpha;
  }

  // 衝撃波のリング: Burst の間だけ。リングごとに、始まりを少しずつずらして、外へ広がる
  if (phase == ClearPhase::Burst && settings_.shockwaveCount > 0) {
    const int count = (std::min)(settings_.shockwaveCount, static_cast<int>(shockwaves_.size()));
    // 最後のリングも、Burst の終わりまでに、広がり切る長さにする
    const float span = (std::max)(1.0f - settings_.shockwaveStagger * static_cast<float>(count - 1), kMinShockwaveSpan);
    for (int i = 0; i < count; ++i) {
      const float t = Saturate((p - settings_.shockwaveStagger * static_cast<float>(i)) / span);
      if (t <= 0.0f) continue;
      const float ringRadius = settings_.maxRadius * Lerp(1.0f, settings_.shockwaveGrow, EaseOutCubic(t));
      const float remainRing = 1.0f - t;
      const float ringAlpha = settings_.shockwaveAlpha * remainRing * remainRing;
      if (ringAlpha <= 0.0f) continue;
      PlaceSphere_(shockwaves_[i], center, ringRadius, settings_.shockwaveColor, ringAlpha);
      shockwaveAlpha_[i] = ringAlpha;
    }
  }
}

void BossExplosion::Draw(Renderer *renderer) {
  if (!ready_ || !renderer) return;
  // 半透明のエフェクト用パイプライン(アルファ合成、深度は書かない)で描く。深度を書かないので、描く順で、重なりが決まる。
  // 奥から手前へ: ハロ -> 核 -> リング(ハロは、核の外側へにじむので、核の下に敷く)。
  if (haloAlpha_ > 0.0f) renderer->DrawEffectModel(&halo_, false);
  // 核は、濃いあいだ、深度を書く(深度ありで描く粒が、球の中・裏側で隠れる)。ハロとリングは薄い光なので、書かない
  if (coreAlpha_ > 0.0f) renderer->DrawEffectModel(&core_, coreAlpha_ >= kCoreDepthMinAlpha);
  for (size_t i = 0; i < shockwaves_.size(); ++i) {
    if (shockwaveAlpha_[i] > 0.0f) renderer->DrawEffectModel(&shockwaves_[i], false);
  }
}

void BossExplosion::DrawFlash(Renderer *renderer, const Vector3 &eye, const Vector3 &target) {
  if (!renderer) return;
  flash_.DrawFlash(renderer, eye, target);
}
