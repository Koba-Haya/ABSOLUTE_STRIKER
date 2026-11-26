#include "PeelPhase.h"
#include "ClearPhaseMath.h"
#include "BurstPhase.h"

float PeelPhase::GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const {
  return ClearPhaseMath::SlowMotionScale(settings, totalElapsed);
}

std::unique_ptr<IClearPhase> PeelPhase::Next_(const ClearSettings &settings, float carriedSeconds) const {
  (void)settings;
  return std::make_unique<BurstPhase>(carriedSeconds);
}
