#include "TimedClearPhase.h"
#include <algorithm>

std::unique_ptr<IClearPhase> TimedClearPhase::Advance(const ClearSettings &settings, float deltaTime) {
  elapsed_ += deltaTime;
  const float length = Length_(settings);
  if (elapsed_ < length) {
    return nullptr;
  }
  // 長さを超えた分は、次の段階の経過として引き継ぐ(1 フレームで複数の段階を越えても、時間がずれない)
  return Next_(settings, elapsed_ - length);
}

float TimedClearPhase::GetProgress(const ClearSettings &settings) const {
  const float length = Length_(settings);
  return length > 0.0f ? std::clamp(elapsed_ / length, 0.0f, 1.0f) : 1.0f;
}
