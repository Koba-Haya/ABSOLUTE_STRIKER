#include "DeathHitStopPhase.h"
#include "DeathPhaseMath.h"
#include "DeathCrumblePhase.h"

float DeathHitStopPhase::GetWorldTimeScale(const PlayerDeathSettings &settings, float totalElapsed) const {
  (void)settings;
  (void)totalElapsed;
  return DeathPhaseMath::kStoppedTimeScale;
}

std::unique_ptr<IDeathPhase> DeathHitStopPhase::Next_(float carriedSeconds) const {
  return std::make_unique<DeathCrumblePhase>(carriedSeconds);
}
