#include "ClearPhaseMath.h"
#include <algorithm>

namespace ClearPhaseMath {

float SmoothStep(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float Ratio(float elapsed, float length) {
  return length > 0.0f ? elapsed / length : 1.0f;
}

float SlowMotionScale(const ClearSettings &settings, float totalElapsed) {
  const float sinceStop = std::max(0.0f, totalElapsed - settings.hitStopSeconds);
  return settings.slowMotionScale * SmoothStep(Ratio(sinceStop, settings.slowMotionRampSeconds));
}

} // namespace ClearPhaseMath
