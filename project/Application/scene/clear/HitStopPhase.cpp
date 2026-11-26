#include "HitStopPhase.h"
#include "ClearPhaseMath.h"
#include "CrackPhase.h"

float HitStopPhase::GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const {
  (void)settings;
  (void)totalElapsed;
  return ClearPhaseMath::kStoppedTimeScale;
}

std::unique_ptr<IClearPhase> HitStopPhase::Next_(const ClearSettings &settings, float carriedSeconds) const {
  (void)settings;
  return std::make_unique<CrackPhase>(carriedSeconds);
}
