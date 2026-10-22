#include "StageStartDirector.h"

#include "Method.h"

#include <algorithm>

namespace {
// start から duration 秒かけて進む区間の、0 〜 1 の進み具合。duration が 0 以下なら、すぐ 1 にする
float CalcProgress(float elapsed, float start, float duration) {
  if (duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp((elapsed - start) / duration, 0.0f, 1.0f);
}

// 0 〜 1 を、始まりと終わりがなめらかな S 字(smoothstep)に変える。
// 傾きが始まりと終わりで 0 になるので、カメラが急に動き出したり、急に止まったりしない
float SmoothStep(float t) {
  return t * t * (3.0f - 2.0f * t);
}
} // namespace

void StageStartDirector::Begin(const StageStartSettings &settings) {
  settings_ = settings;
  elapsed_ = 0.0f;
  active_ = true;
}

void StageStartDirector::Update(float deltaTime) {
  if (!active_) {
    return;
  }
  elapsed_ += deltaTime;
  if (elapsed_ >= TotalSeconds_()) {
    active_ = false;
  }
}

StageStartPhase StageStartDirector::GetPhase() const {
  if (!active_) {
    return StageStartPhase::Idle;
  }
  if (elapsed_ < settings_.pullBackSeconds) {
    return StageStartPhase::PullBack;
  }
  if (elapsed_ < settings_.pullBackSeconds + settings_.holdSeconds) {
    return StageStartPhase::Hold;
  }
  return StageStartPhase::Return;
}

bool StageStartDirector::HasReachedBreak() const {
  if (GetPhase() != StageStartPhase::Return) {
    return false;
  }
  const float returnStart = settings_.pullBackSeconds + settings_.holdSeconds;
  return elapsed_ - returnStart >= settings_.breakSeconds;
}

float StageStartDirector::GetPhaseProgress() const {
  switch (GetPhase()) {
  case StageStartPhase::PullBack:
    return CalcProgress(elapsed_, 0.0f, settings_.pullBackSeconds);
  case StageStartPhase::Hold:
    return CalcProgress(elapsed_, settings_.pullBackSeconds, settings_.holdSeconds);
  case StageStartPhase::Return:
    return CalcProgress(elapsed_, settings_.pullBackSeconds + settings_.holdSeconds, settings_.returnSeconds);
  case StageStartPhase::Idle:
  default:
    return 0.0f;
  }
}

float StageStartDirector::GetReturnElapsedSeconds() const {
  if (GetPhase() != StageStartPhase::Return) {
    return 0.0f;
  }
  return elapsed_ - (settings_.pullBackSeconds + settings_.holdSeconds);
}

float StageStartDirector::GetBoost() const {
  if (GetPhase() != StageStartPhase::Return) {
    return 0.0f;
  }
  return BoostAt(GetReturnElapsedSeconds());
}

float StageStartDirector::BoostAt(float returnElapsedSeconds) const {
  const float breakSeconds = settings_.breakSeconds;
  if (returnElapsedSeconds <= 0.0f || breakSeconds <= 0.0f) {
    return 0.0f;
  }
  if (returnElapsedSeconds <= breakSeconds) {
    // 板を割る時刻へ向けて、だんだん速く上がる(2 次曲線)
    const float t = returnElapsedSeconds / breakSeconds;
    return t * t;
  }
  // 割れたあとは、「戻る」段階の終わりへ向けて、なめらかに 0 へ戻る
  const float decaySeconds = settings_.returnSeconds - breakSeconds;
  if (decaySeconds <= 0.0f) {
    return 0.0f;
  }
  return 1.0f - SmoothStep(CalcProgress(returnElapsedSeconds, breakSeconds, decaySeconds));
}

StageStartPose StageStartDirector::GetPose() const {
  return MakePose_(CalcBlend_());
}

StageStartPose StageStartDirector::GetHoldPose() const {
  return MakePose_(1.0f);
}

void StageStartDirector::ApplyPose(const StageStartPose &pose, const Vector3 &eye, const Vector3 &target, Vector3 &outEye,
                                   Vector3 &outTarget) {
  const Vector3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
  outEye = {eye.x - forward.x * pose.backOffset, eye.y - forward.y * pose.backOffset + pose.riseOffset,
            eye.z - forward.z * pose.backOffset};
  outTarget = {target.x + forward.x * pose.lookAheadOffset, target.y + forward.y * pose.lookAheadOffset,
               target.z + forward.z * pose.lookAheadOffset};
}

StageStartPose StageStartDirector::MakePose_(float blend) const {
  StageStartPose pose;
  pose.backOffset = settings_.pullBackDistance * blend;
  pose.riseOffset = settings_.riseHeight * blend;
  pose.lookAheadOffset = settings_.lookAheadDistance * blend;
  pose.fovScale = 1.0f + (settings_.fovScale - 1.0f) * blend;
  return pose;
}

float StageStartDirector::TotalSeconds_() const {
  return settings_.pullBackSeconds + settings_.holdSeconds + settings_.returnSeconds;
}

float StageStartDirector::CalcBlend_() const {
  switch (GetPhase()) {
  case StageStartPhase::PullBack:
    // 0 -> 1 へ、なめらかに引く
    return SmoothStep(CalcProgress(elapsed_, 0.0f, settings_.pullBackSeconds));
  case StageStartPhase::Hold:
    return 1.0f;
  case StageStartPhase::Return: {
    // 1 -> 0 へ、なめらかに戻る
    const float start = settings_.pullBackSeconds + settings_.holdSeconds;
    return 1.0f - SmoothStep(CalcProgress(elapsed_, start, settings_.returnSeconds));
  }
  case StageStartPhase::Idle:
  default:
    return 0.0f;
  }
}
