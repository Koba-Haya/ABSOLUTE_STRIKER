#include "SceneTransition.h"

#include <algorithm>

namespace {
// 0〜1 を、始まりと終わりがなめらかな曲線(smoothstep)にする
float Smooth(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// edge0 から edge1 にかけて 0 から 1 へなめらかに変わる曲線(edge1 <= edge0 なら、edge0 で切り替わる段)
float SmoothBetween(float edge0, float edge1, float t) {
  if (edge1 <= edge0) {
    return t >= edge0 ? 1.0f : 0.0f;
  }
  return Smooth((t - edge0) / (edge1 - edge0));
}
} // namespace

void SceneTransition::Begin(const TransitionSpec &spec) {
  spec_ = spec;
  totalElapsed_ = 0.0f;
  switchRequested_ = false;
  EnterPhase_(Phase::Out);
  // 覆う時間が 0 なら、始めたその場で完全に覆っているので、すぐシーンを入れ替えてよい
  if (spec_.outSeconds <= 0.0f) {
    switchRequested_ = true;
    EnterPhase_(Phase::Hold);
  }
}

void SceneTransition::Cancel() {
  phase_ = Phase::Idle;
  elapsed_ = 0.0f;
  switchRequested_ = false;
}

void SceneTransition::EnterPhase_(Phase next) {
  phase_ = next;
  elapsed_ = 0.0f;
}

float SceneTransition::Progress_(float duration) const {
  if (duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(elapsed_ / duration, 0.0f, 1.0f);
}

void SceneTransition::Update(float deltaTime) {
  if (phase_ == Phase::Idle) {
    return;
  }
  elapsed_ += deltaTime;
  totalElapsed_ += deltaTime;

  switch (phase_) {
  case Phase::Out:
    if (elapsed_ >= spec_.outSeconds) {
      // 覆い切った。このフレームにシーンを入れ替えてもらい、Hold へ移る
      switchRequested_ = true;
      EnterPhase_(Phase::Hold);
    }
    break;
  case Phase::Hold:
    if (elapsed_ >= spec_.holdSeconds) {
      EnterPhase_(Phase::In);
    }
    break;
  case Phase::In:
    if (elapsed_ >= spec_.inSeconds) {
      Cancel();
    }
    break;
  case Phase::Idle:
    break;
  }
}

bool SceneTransition::ConsumeSwitchRequest() {
  const bool requested = switchRequested_;
  switchRequested_ = false;
  return requested;
}

TransitionStyle SceneTransition::GetStyle() const {
  TransitionStyle style = spec_.style;
  if (phase_ == Phase::Hold || phase_ == Phase::In) {
    style.slabBottom = spec_.slabBottomAfter;
    style.slabTop = spec_.slabTopAfter;
  }
  // 水平な層(雲海など)は、入れ替え前のシーンの世界の高さ。入れ替え後は、別のシーンなので使わない
  if (phase_ == Phase::Hold || phase_ == Phase::In) {
    style.mist.backgroundPlane = false;
  }
  style.mist.wetness = Wetness_(style.mist);
  // 水滴が付き始めた時刻(覆う区間の wetRiseStart の位置)からの経過
  style.mist.lensTime = (std::max)(0.0f, totalElapsed_ - style.mist.wetRiseStart * spec_.outSeconds);
  return style;
}

float SceneTransition::Wetness_(const MistLensStyle &mist) const {
  switch (phase_) {
  case Phase::Out:
    return SmoothBetween(mist.wetRiseStart, mist.wetRiseEnd, Progress_(spec_.outSeconds));
  case Phase::Hold:
    return 1.0f;
  case Phase::In:
    // 付くのは一気に、乾くのは長く残る(乾く区間の頭は、まだ濡れたまま)
    return 1.0f - SmoothBetween(mist.wetDrainStart, mist.wetDrainEnd, Progress_(spec_.inSeconds));
  case Phase::Idle:
    break;
  }
  return 0.0f;
}

float SceneTransition::GetRevealProgress() const {
  switch (phase_) {
  case Phase::Out:
  case Phase::Hold:
    return 0.0f;
  case Phase::In:
    return Progress_(spec_.inSeconds);
  case Phase::Idle:
    break;
  }
  return 1.0f;
}

float SceneTransition::GetCover() const {
  switch (phase_) {
  case Phase::Out:
    return Smooth(Progress_(spec_.outSeconds));
  case Phase::Hold:
    return 1.0f;
  case Phase::In:
    return 1.0f - Smooth(Progress_(spec_.inSeconds));
  case Phase::Idle:
    break;
  }
  return 0.0f;
}
