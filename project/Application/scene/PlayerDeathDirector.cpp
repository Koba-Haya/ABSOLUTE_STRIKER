#include "PlayerDeathDirector.h"
#include <algorithm>
#include <cmath>

namespace {
// 0 〜 1 の値を、なめらかに(両端でゆっくり)変える
float SmoothStep(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
} // namespace

void PlayerDeathDirector::Begin(const PlayerDeathSettings& settings, const PlayerDeathStart& start) {
  if (IsActive()) return;
  settings_ = settings;
  start_ = start;
  elapsed_ = 0.0f;
  coverStartPending_ = false;
  crumbleStartPending_ = false;
  EnterPhase_(PlayerDeathPhase::HitStop);
}

void PlayerDeathDirector::EnterPhase_(PlayerDeathPhase phase) {
  phase_ = phase;
  phaseElapsed_ = 0.0f;
  if (phase == PlayerDeathPhase::Crumble) crumbleStartPending_ = true;
  if (phase == PlayerDeathPhase::Cover) coverStartPending_ = true;
}

float PlayerDeathDirector::PhaseLength_(PlayerDeathPhase phase) const {
  switch (phase) {
  case PlayerDeathPhase::HitStop: return settings_.hitStopSeconds;
  case PlayerDeathPhase::Crumble: return settings_.crumbleSeconds;
  case PlayerDeathPhase::Fall: return settings_.fallSeconds;
  case PlayerDeathPhase::Impact: return settings_.impactSeconds;
  default: return 0.0f; // Idle / Cover は、時間で終わらない
  }
}

void PlayerDeathDirector::Update(float deltaTime, bool canSkip, bool skipRequested) {
  if (!IsActive()) return;

  elapsed_ += deltaTime;

  // スキップ: 猶予を過ぎていれば、Cover(遷移を要求する段階)へ直接飛ぶ
  if (canSkip && skipRequested && elapsed_ >= settings_.skipGuardSeconds && phase_ != PlayerDeathPhase::Cover) {
    // 途中の段階を飛ばしても、崩壊の開始の合図は 1 回出す(自機をきりもみへ切り替えるため)
    if (phase_ == PlayerDeathPhase::HitStop) crumbleStartPending_ = true;
    EnterPhase_(PlayerDeathPhase::Cover);
    return;
  }

  phaseElapsed_ += deltaTime;

  // 時間で終わるサブフェーズを、順に進める(1 フレームで複数を越えても、余りを次へ持ち越す)
  while (phase_ != PlayerDeathPhase::Cover) {
    const float length = PhaseLength_(phase_);
    if (phaseElapsed_ < length) break;
    phaseElapsed_ -= length;
    switch (phase_) {
    case PlayerDeathPhase::HitStop: EnterPhase_(PlayerDeathPhase::Crumble); break;
    case PlayerDeathPhase::Crumble: EnterPhase_(PlayerDeathPhase::Fall); break;
    case PlayerDeathPhase::Fall: EnterPhase_(PlayerDeathPhase::Impact); break;
    case PlayerDeathPhase::Impact: EnterPhase_(PlayerDeathPhase::Cover); break;
    default: return;
    }
  }
}

float PlayerDeathDirector::GetPhaseProgress() const {
  if (phase_ == PlayerDeathPhase::Idle) return 0.0f;
  if (phase_ == PlayerDeathPhase::Cover) return 1.0f;
  const float length = PhaseLength_(phase_);
  return length > 0.0f ? std::clamp(phaseElapsed_ / length, 0.0f, 1.0f) : 1.0f;
}

float PlayerDeathDirector::GetWorldTimeScale() const {
  if (!IsActive()) return 1.0f;
  if (phase_ == PlayerDeathPhase::HitStop) return 0.0f;
  // ヒットストップが終わってから、スローモーションの速さへ、なめらかに移る
  const float sinceStop = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);
  const float ramp = settings_.slowMotionRampSeconds > 0.0f ? sinceStop / settings_.slowMotionRampSeconds : 1.0f;
  return settings_.slowMotionScale * SmoothStep(ramp);
}

PlayerDeathPose PlayerDeathDirector::GetPlayerPose() const {
  PlayerDeathPose pose;
  pose.position = start_.position;
  pose.rotate = start_.rotate;
  if (!IsActive()) return pose;

  // デバッグ用: 位置も向きも動かさない
  if (settings_.debugHoldPlayerPosition) return pose;

  // ヒットストップの間は、死亡の瞬間の位置で止まる。落下は、そのあとの時間で数える
  const float t = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);

  // 前へ進む勢いは、指数的に減る。距離 = 初速 * (1 - e^(-k t)) / k(速さの積分)
  const float k = settings_.forwardDecayPerSecond;
  const float forwardSpeed = start_.railSpeed * settings_.forwardSpeedScale;
  const float forwardDistance = k > 0.0f ? forwardSpeed * (1.0f - std::exp(-k * t)) / k : forwardSpeed * t;
  // 重力で落ちる距離 = 1/2 g t^2
  const float fallDistance = 0.5f * settings_.gravity * t * t;

  pose.position = {start_.position.x + start_.forward.x * forwardDistance,
                   start_.position.y + start_.forward.y * forwardDistance - fallDistance,
                   start_.position.z + start_.forward.z * forwardDistance};

  // 機首が、だんだん下を向く(Transform の x まわりの正の回転が、機首を下げる向き)
  const float nose = settings_.noseDropSeconds > 0.0f ? SmoothStep(t / settings_.noseDropSeconds) : 1.0f;
  pose.rotate.x += settings_.noseDropRadians * nose;
  return pose;
}

Vector3 PlayerDeathDirector::GetPlayerVelocity() const {
  if (!IsActive() || settings_.debugHoldPlayerPosition) return {0.0f, 0.0f, 0.0f};
  const float t = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);
  // 位置の式(GetPlayerPose)を、時間で微分したもの: 前へ進む速さは指数的に減り、下へ落ちる速さは重力に比例して増える
  const float forwardSpeed = start_.railSpeed * settings_.forwardSpeedScale * std::exp(-settings_.forwardDecayPerSecond * t);
  return {start_.forward.x * forwardSpeed, start_.forward.y * forwardSpeed - settings_.gravity * t, start_.forward.z * forwardSpeed};
}

float PlayerDeathDirector::GetCrumbleProgress() const {
  if (!IsActive() || phase_ == PlayerDeathPhase::HitStop) return 0.0f;
  return phase_ == PlayerDeathPhase::Crumble ? GetPhaseProgress() : 1.0f;
}

PlayerDeathDissolve PlayerDeathDirector::GetDissolve() const {
  PlayerDeathDissolve dissolve;
  if (!IsActive() || phase_ == PlayerDeathPhase::HitStop) return dissolve;

  const float progress = GetCrumbleProgress();
  dissolve.enabled = true;
  dissolve.threshold = settings_.dissolveStartThreshold +
                       (settings_.dissolveEndThreshold - settings_.dissolveStartThreshold) * progress;
  dissolve.edgeRange = settings_.dissolveEdgeRange;
  dissolve.edgeColor = settings_.dissolveEdgeColor;
  return dissolve;
}

bool PlayerDeathDirector::ConsumeCoverStart() {
  const bool pending = coverStartPending_;
  coverStartPending_ = false;
  return pending;
}

bool PlayerDeathDirector::ConsumeCrumbleStart() {
  const bool pending = crumbleStartPending_;
  crumbleStartPending_ = false;
  return pending;
}
