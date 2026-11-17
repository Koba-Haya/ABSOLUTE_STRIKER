#include "ClearDirector.h"
#include <algorithm>

namespace {
// 0 〜 1 の値を、なめらかに(両端でゆっくり)変える
float SmoothStep(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// 経過 elapsed が、長さ length のうちどこまで進んだか(0 〜 1)。長さが 0 以下なら、すでに終わった扱い(1)
float Ratio(float elapsed, float length) {
  return length > 0.0f ? elapsed / length : 1.0f;
}
} // namespace

void ClearDirector::Begin(const ClearSettings &settings) {
  if (IsActive()) return;
  settings_ = settings;
  elapsed_ = 0.0f;
  coverStartPending_ = false;
  EnterPhase_(ClearPhase::HitStop);
}

void ClearDirector::Reset() {
  phase_ = ClearPhase::Idle;
  elapsed_ = 0.0f;
  phaseElapsed_ = 0.0f;
  coverStartPending_ = false;
}

float ClearDirector::GetPhaseEndTime(ClearPhase phase) const {
  float end = settings_.hitStopSeconds;
  if (phase == ClearPhase::HitStop) return end;
  end += settings_.crackSeconds;
  if (phase == ClearPhase::Crack) return end;
  end += settings_.peelSeconds;
  if (phase == ClearPhase::Peel) return end;
  end += settings_.burstSeconds;
  if (phase == ClearPhase::Burst) return end;
  return end + settings_.flowSeconds; // Flow と Cover は、同じ時刻(Flow が終わったところで Cover へ入る)
}

void ClearDirector::EnterPhase_(ClearPhase phase) {
  phase_ = phase;
  phaseElapsed_ = 0.0f;
  if (phase == ClearPhase::Cover) coverStartPending_ = true;
}

float ClearDirector::PhaseLength_(ClearPhase phase) const {
  switch (phase) {
  case ClearPhase::HitStop: return settings_.hitStopSeconds;
  case ClearPhase::Crack: return settings_.crackSeconds;
  case ClearPhase::Peel: return settings_.peelSeconds;
  case ClearPhase::Burst: return settings_.burstSeconds;
  case ClearPhase::Flow: return settings_.flowSeconds;
  default: return 0.0f; // Idle / Cover は、時間で終わらない
  }
}

void ClearDirector::Update(float deltaTime, bool canSkip, bool skipRequested) {
  if (!IsActive()) return;

  elapsed_ += deltaTime;

  // スキップ: 猶予を過ぎていれば、Cover(遷移を要求する段階)へ直接飛ぶ
  if (canSkip && skipRequested && elapsed_ >= settings_.skipGuardSeconds && phase_ != ClearPhase::Cover) {
    EnterPhase_(ClearPhase::Cover);
    return;
  }

  phaseElapsed_ += deltaTime;

  // 時間で終わるサブフェーズを、順に進める(1 フレームで複数を越えても、余りを次へ持ち越す)
  while (phase_ != ClearPhase::Cover) {
    const float length = PhaseLength_(phase_);
    if (phaseElapsed_ < length) break;
    phaseElapsed_ -= length;
    switch (phase_) {
    case ClearPhase::HitStop: EnterPhase_(ClearPhase::Crack); break;
    case ClearPhase::Crack: EnterPhase_(ClearPhase::Peel); break;
    case ClearPhase::Peel: EnterPhase_(ClearPhase::Burst); break;
    case ClearPhase::Burst: EnterPhase_(ClearPhase::Flow); break;
    case ClearPhase::Flow: EnterPhase_(ClearPhase::Cover); break;
    default: return;
    }
  }
}

float ClearDirector::GetPhaseProgress() const {
  if (phase_ == ClearPhase::Idle) return 0.0f;
  if (phase_ == ClearPhase::Cover) return 1.0f;
  const float length = PhaseLength_(phase_);
  return length > 0.0f ? std::clamp(phaseElapsed_ / length, 0.0f, 1.0f) : 1.0f;
}

float ClearDirector::GetWorldTimeScale() const {
  if (!IsActive()) return 1.0f;
  if (phase_ == ClearPhase::HitStop) return 0.0f;

  // ヒットストップが終わってから、スローモーションの速さへ、なめらかに移る
  const float sinceStop = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);
  const float slow = settings_.slowMotionScale * SmoothStep(Ratio(sinceStop, settings_.slowMotionRampSeconds));

  // Flow に入ってからは、通常の速さ(1)へ戻る。Cover に入ったあと(スキップを含む)は、通常の速さ
  if (phase_ == ClearPhase::Flow) {
    const float recover = SmoothStep(Ratio(phaseElapsed_, settings_.recoverRampSeconds));
    return slow + (1.0f - slow) * recover;
  }
  if (phase_ == ClearPhase::Cover) return 1.0f;
  return slow;
}

bool ClearDirector::ConsumeCoverStart() {
  const bool pending = coverStartPending_;
  coverStartPending_ = false;
  return pending;
}
