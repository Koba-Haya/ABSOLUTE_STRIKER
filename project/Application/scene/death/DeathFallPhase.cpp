#include "DeathFallPhase.h"
#include "DeathImpactPhase.h"

std::unique_ptr<IDeathPhase> DeathFallPhase::Next_(float carriedSeconds) const {
  return std::make_unique<DeathImpactPhase>(carriedSeconds);
}
