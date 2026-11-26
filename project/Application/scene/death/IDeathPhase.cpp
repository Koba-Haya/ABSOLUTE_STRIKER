#include "IDeathPhase.h"
#include "DeathPhaseMath.h"

float IDeathPhase::GetWorldTimeScale(const PlayerDeathSettings &settings, float totalElapsed) const {
  // ヒットストップ以外の段階(覆う段階も含む)は、スローモーションのまま(以前と同じ)
  return DeathPhaseMath::SlowMotionScale(settings, totalElapsed);
}
