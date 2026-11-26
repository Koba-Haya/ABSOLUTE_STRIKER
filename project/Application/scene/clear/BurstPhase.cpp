#include "BurstPhase.h"
#include "ClearPhaseMath.h"
#include "FlowPhase.h"

float BurstPhase::GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const {
  return ClearPhaseMath::SlowMotionScale(settings, totalElapsed);
}

std::unique_ptr<IClearPhase> BurstPhase::Next_(const ClearSettings &settings, float carriedSeconds) const {
  (void)settings;
  return std::make_unique<FlowPhase>(carriedSeconds);
}
