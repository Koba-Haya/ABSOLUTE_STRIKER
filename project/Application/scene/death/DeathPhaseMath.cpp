#include "DeathPhaseMath.h"
#include <algorithm>

namespace DeathPhaseMath {

float SmoothStep(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float SlowMotionScale(const PlayerDeathSettings &settings, float totalElapsed) {
  const float sinceStop = std::max(0.0f, totalElapsed - settings.hitStopSeconds);
  const float ramp = settings.slowMotionRampSeconds > 0.0f ? sinceStop / settings.slowMotionRampSeconds : 1.0f;
  return settings.slowMotionScale * SmoothStep(ramp);
}

} // namespace DeathPhaseMath
