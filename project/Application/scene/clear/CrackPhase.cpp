#include "CrackPhase.h"
#include "ClearPhaseMath.h"
#include "PeelPhase.h"

float CrackPhase::GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const {
  return ClearPhaseMath::SlowMotionScale(settings, totalElapsed);
}

std::unique_ptr<IClearPhase> CrackPhase::Next_(const ClearSettings &settings, float carriedSeconds) const {
  (void)settings;
  return std::make_unique<PeelPhase>(carriedSeconds);
}
