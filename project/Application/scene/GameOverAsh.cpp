#define NOMINMAX // Windows の min / max マクロが std::min / std::max と衝突するのを防ぐ
#include "GameOverAsh.h"

#include "Method.h"
#include "Renderer.h"
#include "graphics/particle/GPUParticleManager.h"

#include <algorithm>
#include <cmath>

namespace {
// 渦の軸(真上。巣のまわりを水平に回す)
const Vector3 kSwirlAxis{0.0f, 1.0f, 0.0f};

float Smooth(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// 初期化の直後、粒の全消去をもう一度要求するフレーム数(前の演出の粒が、最初の数フレームに残らないための念押し)
constexpr uint32_t kRepeatClearFrames = 2;
constexpr uint32_t kGroundSegments = 4;           // 地面の吹き飛びの方向の数(バーストのプールの大きさに合わせる)
constexpr float kGroundSegmentOffset = 0.6f;      // 吹き飛びの出る位置を、巣の中心から外へずらす量(ユニット)
constexpr float kGroundNoiseRatio = 0.5f;         // 外向きの速さの幅に対する、向きのばらつきの速さの割合
constexpr float kTwoPi = 6.2831853f;
} // namespace

GameOverAshSettings::GameOverAshSettings() {
  // 灰の見た目と漂いを、この画面用にする(色は鱗粉の既定のまま = 橙の火の粉が灰へ冷める)
  dust.lifeMin = GameOverAshDefaults::kLifeMin;
  dust.lifeMax = GameOverAshDefaults::kLifeMax;
  dust.sizeMin = GameOverAshDefaults::kSizeMin;
  dust.sizeMax = GameOverAshDefaults::kSizeMax;
  dust.speedMin = GameOverAshDefaults::kSpeedMin;
  dust.speedMax = GameOverAshDefaults::kSpeedMax;
  dust.gravity = GameOverAshDefaults::kDriftGravity;
  dust.dragPerSecond = GameOverAshDefaults::kDriftDrag;
  dust.turbulenceStrength = GameOverAshDefaults::kDriftTurbulenceStrength;
  dust.turbulenceFrequency = GameOverAshDefaults::kDriftTurbulenceFrequency;
  dust.turbulenceSpeed = GameOverAshDefaults::kDriftTurbulenceSpeed;
  dust.inheritVelocityScale = 0.0f; // 発生源は動かない(カメラが動くだけ)ので、引き継ぎは使わない
}

void GameOverAsh::Initialize(const Vector3& nestCenter) {
  if (initialized_) return;
  nestCenter_ = nestCenter;
  dust_.Initialize(settings_.dust);
  core_.Initialize(settings_.core);
  frameCount_ = 0;
  glowTime_ = 0.0f;
  gatherEase_ = 0.0f;
  phase_ = GameOverAshPhase::Drifting;
  gatherClock_ = 0.0f;
  gatherSpeedScale_ = 1.0f;
  coreBoost_ = 0.0f;
  phaseOverridden_ = false;
  initialized_ = true;
}

void GameOverAsh::Shutdown() {
  if (!initialized_) return;
  dust_.Shutdown();
  core_.Shutdown();
  initialized_ = false;
}

void GameOverAsh::GetCamera_(Vector3& eye, Vector3& forward) const {
  // カメラの世界行列 = ビュー行列の逆。行ごとに、右・上・前・位置(行ベクトル規約。GPUParticleManager のビルボードと同じ)
  const Matrix4x4 world = Inverse(Renderer::GetInstance()->GetViewMatrix());
  eye = {world.m[3][0], world.m[3][1], world.m[3][2]};
  forward = Normalize(Vector3{world.m[2][0], world.m[2][1], world.m[2][2]});
}

void GameOverAsh::EnterPhase_(GameOverAshPhase phase) {
  phase_ = phase;
  if (phase == GameOverAshPhase::Drifting) {
    gatherClock_ = 0.0f;
  } else if (phase == GameOverAshPhase::Gathering) {
    gatherClock_ = 0.0f;
  } else {
    gatherClock_ = std::max(settings_.gatherSeconds, GameOverAshDefaults::kMinGatherSeconds);
  }
}

void GameOverAsh::SetPhase(GameOverAshPhase phase) {
  phaseOverridden_ = true;
  EnterPhase_(phase);
}

void GameOverAsh::ClearPhaseOverride() { phaseOverridden_ = false; }

float GameOverAsh::GetGatherProgress() const {
  const float length = std::max(settings_.gatherSeconds, GameOverAshDefaults::kMinGatherSeconds);
  return std::clamp(gatherClock_ / length, 0.0f, 1.0f);
}

void GameOverAsh::DrawGlow() {
  if (!initialized_) return;
  // 熱を帯びる中心は核の位置。集まりが進むほど、巣に近い粒が熱くなる(灰が巣で再び熾る)
  GPUGlowParams params = settings_.glow;
  params.hotCenter = core_.GetPosition();
  params.hotRadius = settings_.hotRadius;
  params.hotBoost = settings_.hotBoostMax * gatherEase_;
  params.time = glowTime_;
  Renderer::GetInstance()->DrawGPUParticleGlow(params);
}

void GameOverAsh::ReleaseUpward(uint32_t count, float speed) {
  dust_.Burst(core_.GetPosition(), count, core_.GetSettings().size * 0.25f, {0.0f, speed, 0.0f});
}

void GameOverAsh::Update(float deltaTime, float settledSeconds) {
  if (!initialized_) return;

  // 初期化の直後は、前の演出の粒が残っていないことを念押しする(全消去は、次の粒の更新で行われる)
  if (frameCount_ < kRepeatClearFrames) {
    GPUParticleManager::GetInstance()->ClearAllParticles();
  }

  // 1. 状態の進行。外から指示されていなければ、降下が始まってからの秒で、漂いから集まりへ進む
  if (!phaseOverridden_) {
    // settledSeconds が 0 の間は、まだ降下が始まっていない(遷移が晴れきっていない)ので、開始時刻が 0 でも漂いのまま
    if (settledSeconds <= 0.0f || settledSeconds < settings_.gatherStartSeconds) {
      if (phase_ != GameOverAshPhase::Drifting) EnterPhase_(GameOverAshPhase::Drifting); // 降下のやり直しなど
    } else if (phase_ == GameOverAshPhase::Drifting) {
      EnterPhase_(GameOverAshPhase::Gathering);
    }
  }
  if (phase_ != GameOverAshPhase::Drifting) {
    gatherClock_ += deltaTime * std::max(gatherSpeedScale_, 0.0f);
    if (phase_ == GameOverAshPhase::Gathering && GetGatherProgress() >= 1.0f) {
      phase_ = GameOverAshPhase::Glowing;
    }
  }
  const float gather = phase_ == GameOverAshPhase::Drifting ? 0.0f : Smooth(GetGatherProgress());
  const bool glowing = phase_ == GameOverAshPhase::Glowing;
  gatherEase_ = gather;
  glowTime_ += deltaTime;

  // 2. 灰の設定(調整 UI が書き換えた値を反映)。集まるにつれて、空気抵抗を、振動せずに収束する値へ寄せる
  ScaleDustSettings look = settings_.dust;
  look.dragPerSecond = Lerp(settings_.dust.dragPerSecond, settings_.gatherDrag, gather);
  dust_.GetSettings() = look;

  // 3. 発生: カメラの前方の球から、続けて出す(近景から遠景まで)。灯りの間は量を減らす
  Vector3 eye;
  Vector3 forward;
  GetCamera_(eye, forward);
  const Vector3 center{eye.x + forward.x * settings_.spawnAheadDistance, eye.y + forward.y * settings_.spawnAheadDistance,
                       eye.z + forward.z * settings_.spawnAheadDistance};
  const float rate = settings_.spawnRate * (glowing ? settings_.glowSpawnRateScale : 1.0f) * std::max(spawnScale_, 0.0f);
  dust_.SetAmbient(center, settings_.spawnRadius, rate);

  // 4. 動き: 巣(核の位置)へ、ばねで収束させながら、渦で回す。集まりの進みの 2 乗で立ち上げる(最初はほとんど漂ったまま)
  const Vector3 target = core_.GetPosition();
  if (gather > 0.0f) {
    const float ramp = gather * gather;
    dust_.SetConverge(target, settings_.convergeStiffness * ramp * std::max(convergeScale_, 0.0f));
    dust_.SetSwirl(target, kSwirlAxis, settings_.swirlStrength * ramp * (glowing ? settings_.glowSwirlScale : 1.0f));
  }

  // 5. 核: 集まった量(集まりの進み)に応じて強くなり、灯りの間は脈打つ。外からの boost を足す
  const float coreProgress = Smooth((GetGatherProgress() - settings_.coreEaseStart) / std::max(1.0f - settings_.coreEaseStart, GameOverAshDefaults::kMinGatherSeconds));
  const float intensity = (phase_ == GameOverAshPhase::Drifting ? 0.0f : settings_.coreIntensity * coreProgress) + coreBoost_;
  core_.GetSettings() = settings_.core;
  core_.GetSettings().size *= std::max(coreSizeScale_, 0.0f); // 溜めで光を圧縮する
  // 核の中心: 巣から、外から指示された位置(蝶の胴体)へ。核の光は中心から lift だけ上に出るので、その分を引いておく
  const float follow = std::clamp(followBlend_, 0.0f, 1.0f);
  const Vector3 followCenter{followPosition_.x, followPosition_.y - settings_.core.lift, followPosition_.z};
  const Vector3 coreCenter{Lerp(nestCenter_.x, followCenter.x, follow), Lerp(nestCenter_.y, followCenter.y, follow),
                           Lerp(nestCenter_.z, followCenter.z, follow)};
  core_.SetState(coreCenter, intensity, glowing ? 1.0f : coreProgress);
  core_.Update(deltaTime);

  // 再生の色のバースト: このフレームだけ、鱗粉の設定を burst の値にして出す(粒は出た時点の色・速さを持つので、次のフレームの通常の設定には影響しない)
  if (burstPending_) {
    ScaleDustSettings& dustSettings = dust_.GetSettings();
    dustSettings.emberColor = burst_.startColor;
    dustSettings.ashColor = burst_.endColor;
    dustSettings.speedMin = burst_.speedMin;
    dustSettings.speedMax = burst_.speedMax;
    dustSettings.lifeMin = burst_.lifeMin;
    dustSettings.lifeMax = burst_.lifeMax;
    dustSettings.sizeMin = burst_.sizeMin;
    dustSettings.sizeMax = burst_.sizeMax;
    dustSettings.glowOwnColor = burst_.glowOwnColor;
    dust_.Burst(burstCenter_, burst_.count, burst_.radius, {0.0f, burst_.upward, 0.0f});
    burstPending_ = false;
  } else if (groundStage_ == 2) {
    // 地面の吹き飛び: 巣の中心のまわりの 4 方向から、外側へ(低い角度)。4 つのバーストは 1 フレームに出せる数(バーストのプール)に収まる
    // (4 つとも、同じフレームの設定 = 土から灰の色を使う)
    ScaleDustSettings& dustSettings = dust_.GetSettings();
    dustSettings.emberColor = ground_.startColor;
    dustSettings.ashColor = ground_.endColor;
    dustSettings.speedMin = 0.0f;
    dustSettings.speedMax = (ground_.speedMax - ground_.speedMin) * kGroundNoiseRatio; // 向きのばらつき(外向きの速さに足す)
    dustSettings.lifeMin = ground_.lifeMin;
    dustSettings.lifeMax = ground_.lifeMax;
    dustSettings.sizeMin = ground_.sizeMin;
    dustSettings.sizeMax = ground_.sizeMax;
    dustSettings.glowOwnColor = false;
    const float outwardSpeed = 0.5f * (ground_.speedMin + ground_.speedMax);
    const uint32_t perSegment = std::max<uint32_t>(1u, ground_.count / kGroundSegments);
    for (uint32_t i = 0; i < kGroundSegments; ++i) {
      // 45 度ずつずらした 4 方向(軸に揃うと、画面で十字に見えるため)
      const float angle = (static_cast<float>(i) + 0.5f) * (kTwoPi / static_cast<float>(kGroundSegments));
      const Vector3 direction{std::cos(angle), 0.0f, std::sin(angle)};
      const Vector3 origin{groundCenter_.x + direction.x * kGroundSegmentOffset, groundCenter_.y, groundCenter_.z + direction.z * kGroundSegmentOffset};
      dust_.Burst(origin, perSegment, ground_.radius,
                  {direction.x * outwardSpeed, ground_.upward, direction.z * outwardSpeed});
    }
    groundStage_ = 0;
  }
  if (groundStage_ == 1) groundStage_ = 2; // 地面の吹き飛びは、バーストの次のフレームに出す

  dust_.Update();
  ++frameCount_;
}
