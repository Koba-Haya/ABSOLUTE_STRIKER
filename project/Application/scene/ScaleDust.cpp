#define NOMINMAX // Windows の min / max マクロが std::min / std::max と衝突するのを防ぐ
#include "ScaleDust.h"

#include "graphics/particle/GPUParticleManager.h"

#include <algorithm>
#include <cmath>

namespace {
// 長さがこれ以下の力は、向きが決められないので、無いものとして扱う
constexpr float kMinForceLength = 1e-4f;

float LengthOf(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// 設定と、発生源の速度から、GPU パーティクルのスタイルを作る。
// 発生源の速度は、引き継ぐ割合を掛けて baseVelocity にする(蝶と一緒に落ちる粒、破片と一緒に飛ぶ粒が作れる)
GPUParticleManager::ParticleStyle MakeStyle(const ScaleDustSettings& settings, const Vector3& sourceVelocity) {
  GPUParticleManager::ParticleStyle style;
  style.colorStart = settings.emberColor;
  style.colorEnd = settings.ashColor;
  style.coolRatio = settings.coolRatio;
  style.fadePower = settings.fadePower;
  style.glowOwnColor = settings.glowOwnColor;
  style.lifeMin = settings.lifeMin;
  style.lifeMax = std::max(settings.lifeMax, settings.lifeMin);
  style.sizeMin = settings.sizeMin;
  style.sizeMax = std::max(settings.sizeMax, settings.sizeMin);
  style.speedMin = settings.speedMin;
  style.speedMax = std::max(settings.speedMax, settings.speedMin);
  style.baseVelocity = {sourceVelocity.x * settings.inheritVelocityScale, sourceVelocity.y * settings.inheritVelocityScale,
                        sourceVelocity.z * settings.inheritVelocityScale};
  return style;
}

// 量の倍率を掛けた個数(四捨五入。0 以下は 0)
uint32_t ScaledCount(uint32_t count, float amountScale) {
  const float scaled = static_cast<float>(count) * std::max(amountScale, 0.0f);
  return static_cast<uint32_t>(std::lround(scaled));
}
} // namespace

void ScaleDust::Initialize(const ScaleDustSettings& settings) {
  if (IsInitialized()) return;
  settings_ = settings;

  auto* manager = GPUParticleManager::GetInstance();
  // 前の演出(別のシーン、前のプレイ)の粒が残っていると、開始時に勝手な場所に出るので、全て消す
  manager->ClearAllParticles();

  // 常時の発生用の常駐エミッタ。最初は止めておく(SetAmbient が呼ばれている間だけ有効にする)
  GPUParticleManager::EmitterDesc desc;
  desc.styled = true;
  desc.count = 1;
  desc.frequency = ScaleDustDefaults::kAmbientIntervalSeconds;
  emitterIndex_ = manager->CreateEmitter(desc);
  if (IsInitialized()) {
    manager->SetEmitterEnabled(emitterIndex_, false);
  }
  pendingBursts_.clear();
  ambientWasOn_ = false;
}

void ScaleDust::Shutdown() {
  if (!IsInitialized()) return;
  auto* manager = GPUParticleManager::GetInstance();
  manager->ReleaseEmitter(emitterIndex_);
  emitterIndex_ = kNoEmitter;
  ClearFields_();
  pendingBursts_.clear();
  ambientRequested_ = false;
  gatherRequested_ = false;
  swirlRequested_ = false;
  ambientWasOn_ = false;
}

void ScaleDust::ClearFields_() {
  auto* manager = GPUParticleManager::GetInstance();
  const GPUParticleManager::FieldDesc none; // type は None
  for (int slot : {kFieldWind, kFieldTurbulence, kFieldDrag, kFieldGather, kFieldSwirl}) {
    manager->SetField(slot, none);
  }
}

void ScaleDust::Burst(const Vector3& position, uint32_t count, float radius, const Vector3& sourceVelocity) {
  if (!IsInitialized() || count == 0) return;
  if (pendingBursts_.size() >= ScaleDustDefaults::kMaxPendingBursts) return; // 持ち越しが溜まりすぎたら、捨てる
  PendingBurst burst;
  burst.position = position;
  burst.count = count;
  burst.radius = radius;
  burst.velocity = sourceVelocity;
  pendingBursts_.push_back(burst);
}

void ScaleDust::SetAmbient(const Vector3& center, float radius, float ratePerSecond, const Vector3& sourceVelocity) {
  ambientRequested_ = true;
  ambientCenter_ = center;
  ambientRadius_ = radius;
  ambientRate_ = ratePerSecond;
  ambientVelocity_ = sourceVelocity;
}

void ScaleDust::SetGather(const Vector3& point, float strength) {
  gatherRequested_ = true;
  gatherIsSpring_ = false;
  gatherPoint_ = point;
  gatherStrength_ = strength;
}

void ScaleDust::SetConverge(const Vector3& point, float stiffness) {
  gatherRequested_ = true;
  gatherIsSpring_ = true;
  gatherPoint_ = point;
  gatherStrength_ = stiffness;
}

void ScaleDust::SetSwirl(const Vector3& point, const Vector3& axis, float strength) {
  swirlRequested_ = true;
  swirlPoint_ = point;
  swirlAxis_ = axis;
  swirlStrength_ = strength;
}

void ScaleDust::Update() {
  if (!IsInitialized()) return;
  auto* manager = GPUParticleManager::GetInstance();

  // 1. 持ち越しのバースト。1 フレームに出せる数(バーストのプールの大きさ)までを出し、残りは次のフレームへ回す
  const size_t emitNow = std::min<size_t>(pendingBursts_.size(), GPUParticleManager::GetBurstPoolSize());
  for (size_t i = 0; i < emitNow; ++i) {
    const PendingBurst& burst = pendingBursts_[i];
    const uint32_t count = ScaledCount(burst.count, settings_.amountScale);
    if (count > 0) {
      manager->EmitStyledBurst(burst.position, count, burst.radius, MakeStyle(settings_, burst.velocity));
    }
  }
  pendingBursts_.erase(pendingBursts_.begin(), pendingBursts_.begin() + static_cast<std::ptrdiff_t>(emitNow));

  // 2. 常時の発生。一定の間隔でまとめて出す(count 個を、frequency 秒ごと)ので、1 秒あたりの個数から count と間隔を求める
  const float rate = ambientRate_ * std::max(settings_.amountScale, 0.0f);
  const bool ambientOn = ambientRequested_ && rate > ScaleDustDefaults::kMinAmbientRate;
  if (ambientOn) {
    const uint32_t count = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(rate * ScaleDustDefaults::kAmbientIntervalSeconds)));
    GPUParticleManager::EmitterDesc desc;
    desc.shape = GPUParticleManager::EmitterShape::Sphere;
    desc.translate = ambientCenter_;
    desc.radius = ambientRadius_;
    desc.count = count;
    desc.frequency = static_cast<float>(count) / rate;
    desc.styled = true;
    desc.style = MakeStyle(settings_, ambientVelocity_);
    manager->ConfigureEmitter(emitterIndex_, desc);
    manager->SetEmitterEnabled(emitterIndex_, true);
  } else if (ambientWasOn_) {
    manager->SetEmitterEnabled(emitterIndex_, false);
  }
  ambientWasOn_ = ambientOn;

  // 3. 力。重力と風は 1 つの Wind にまとめる(向きと大きさに分ける)
  GPUParticleManager::FieldDesc wind;
  const Vector3 windForce{settings_.wind.x, settings_.wind.y - settings_.gravity, settings_.wind.z};
  const float windLength = LengthOf(windForce);
  if (windLength > kMinForceLength) {
    wind.type = GPUParticleManager::FieldType::Wind;
    wind.direction = windForce;
    wind.strength = windLength;
  }
  manager->SetField(kFieldWind, wind);

  GPUParticleManager::FieldDesc turbulence;
  if (settings_.turbulenceStrength > kMinForceLength) {
    turbulence.type = GPUParticleManager::FieldType::Turbulence;
    turbulence.strength = settings_.turbulenceStrength;
    turbulence.direction = {settings_.turbulenceFrequency, settings_.turbulenceSpeed, 0.0f}; // x: 空間の細かさ、y: 変化の速さ
  }
  manager->SetField(kFieldTurbulence, turbulence);

  GPUParticleManager::FieldDesc drag;
  if (settings_.dragPerSecond > kMinForceLength) {
    drag.type = GPUParticleManager::FieldType::Drag;
    drag.strength = settings_.dragPerSecond;
  }
  manager->SetField(kFieldDrag, drag);

  GPUParticleManager::FieldDesc gather;
  if (gatherRequested_ && gatherStrength_ > kMinForceLength) {
    gather.type = gatherIsSpring_ ? GPUParticleManager::FieldType::Spring : GPUParticleManager::FieldType::Attractor;
    gather.target = gatherPoint_;
    gather.strength = gatherStrength_;
  }
  manager->SetField(kFieldGather, gather);

  GPUParticleManager::FieldDesc swirl;
  if (swirlRequested_ && std::fabs(swirlStrength_) > kMinForceLength) {
    swirl.type = GPUParticleManager::FieldType::Vortex;
    swirl.target = swirlPoint_;
    swirl.direction = swirlAxis_;
    swirl.strength = swirlStrength_;
  }
  manager->SetField(kFieldSwirl, swirl);

  // 4. このフレームの指定を使い切る(次のフレームも続けたいなら、呼び出し側が、また呼ぶ)
  ambientRequested_ = false;
  gatherRequested_ = false;
  swirlRequested_ = false;
}
