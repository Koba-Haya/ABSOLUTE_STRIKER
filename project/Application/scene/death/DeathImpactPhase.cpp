#include "DeathImpactPhase.h"
#include "DeathCoverPhase.h"

std::unique_ptr<IDeathPhase> DeathImpactPhase::Next_(float carriedSeconds) const {
  (void)carriedSeconds; // 覆う段階は時間で進まないので、余りは使わない
  return std::make_unique<DeathCoverPhase>();
}
