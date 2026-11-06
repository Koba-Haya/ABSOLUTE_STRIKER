#define NOMINMAX // Windows の min / max マクロが std::min / std::max と衝突するのを防ぐ
#include "EmberCore.h"

#include "Renderer.h"
#include "graphics/particle/GPUParticleManager.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
Vector3 LerpColor(const Vector3& a, const Vector3& b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
} // namespace

void EmberCore::Initialize(const EmberCoreSettings& settings) {
  if (IsInitialized()) return;
  settings_ = settings;
  GPUParticleManager::EmitterDesc desc;
  desc.styled = true;
  desc.count = 1;
  desc.frequency = EmberCoreDefaults::kEmitInterval;
  emitterIndex_ = GPUParticleManager::GetInstance()->CreateEmitter(desc);
  if (IsInitialized()) {
    GPUParticleManager::GetInstance()->SetEmitterEnabled(emitterIndex_, false);
  }
  intensity_ = 0.0f;
  currentIntensity_ = 0.0f;
  pulseTime_ = 0.0f;
}

void EmberCore::Shutdown() {
  if (lightWasOn_) {
    Renderer::GetInstance()->SetPointLights({}, false);
    lightWasOn_ = false;
  }
  if (!IsInitialized()) return;
  GPUParticleManager::GetInstance()->ReleaseEmitter(emitterIndex_);
  emitterIndex_ = kNoEmitter;
}

void EmberCore::SetState(const Vector3& nestCenter, float intensity, float pulseAmount) {
  center_ = nestCenter;
  intensity_ = std::max(intensity, 0.0f);
  pulseAmount_ = std::clamp(pulseAmount, 0.0f, 1.0f);
}

void EmberCore::Update(float deltaTime) {
  if (!IsInitialized()) return;
  auto* manager = GPUParticleManager::GetInstance();
  pulseTime_ += deltaTime;

  // 脈打ち: 強さに、1 を中心にした sin の振れを掛ける(強さ 0 のときは、0 のまま)
  const float pulse = 1.0f + settings_.pulseDepth * pulseAmount_ * std::sin(pulseTime_ * settings_.pulseSpeed);
  currentIntensity_ = intensity_ * pulse;

  const bool on = currentIntensity_ > EmberCoreDefaults::kMinActiveIntensity;
  if (on) {
    // 大きさ: 強さ 0 で minSizeRatio、1 で 1 倍。1 を超えた分は、boostSizeGain の割合で広がる
    const float base = std::clamp(currentIntensity_, 0.0f, 1.0f);
    const float boost = std::max(currentIntensity_ - 1.0f, 0.0f);
    const float size = settings_.size * (settings_.minSizeRatio + (1.0f - settings_.minSizeRatio) * base + settings_.boostSizeGain * boost);
    // 熱さ: 強さ 0 付近で minHeat(赤寄り)、1 で 1(黄、芯は白)。発光専用の粒は、熱さを colorStart.x で指定する
    const float heat = settings_.minHeat + (1.0f - settings_.minHeat) * base;

    GPUParticleManager::EmitterDesc desc;
    desc.shape = GPUParticleManager::EmitterShape::Sphere;
    desc.translate = GetPosition();
    desc.radius = EmberCoreDefaults::kJitterRadius;
    desc.count = 1;
    desc.frequency = EmberCoreDefaults::kEmitInterval;
    desc.styled = true;
    desc.style.colorStart = {heat, 0.0f, 0.0f};
    desc.style.colorEnd = {heat, 0.0f, 0.0f};
    desc.style.coolRatio = 1.0f;
    desc.style.glowOnly = true;
    desc.style.glowGain = settings_.gainPerIntensity * currentIntensity_;
    desc.style.fadePower = settings_.fadePower;
    desc.style.lifeMin = settings_.particleLife;
    desc.style.lifeMax = settings_.particleLife;
    desc.style.sizeMin = size;
    desc.style.sizeMax = size;
    desc.style.speedMin = 0.0f;
    desc.style.speedMax = 0.0f;
    manager->ConfigureEmitter(emitterIndex_, desc);
  }
  manager->SetEmitterEnabled(emitterIndex_, on);

  // 巣の島への点光源(橙)。強さに比例して強くなる。核が消えたら、光も消す
  if (settings_.lightEnabled && on) {
    PointLight light;
    light.color = LerpColor(settings_.dimColor, settings_.hotColor, std::clamp(currentIntensity_, 0.0f, 1.0f));
    light.position = GetPosition();
    light.intensity = settings_.lightIntensity * currentIntensity_;
    light.radius = settings_.lightRadius;
    light.decay = EmberCoreDefaults::kLightDecay;
    light.enabled = true;
    Renderer::GetInstance()->SetPointLights(std::vector<PointLight>(1, light), true);
    lightWasOn_ = true;
  } else if (lightWasOn_) {
    Renderer::GetInstance()->SetPointLights({}, false);
    lightWasOn_ = false;
  }
}
