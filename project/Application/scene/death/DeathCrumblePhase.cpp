#include "DeathCrumblePhase.h"
#include "DeathFallPhase.h"

std::unique_ptr<IDeathPhase> DeathCrumblePhase::Next_(float carriedSeconds) const {
  return std::make_unique<DeathFallPhase>(carriedSeconds);
}
