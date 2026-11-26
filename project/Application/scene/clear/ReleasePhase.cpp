#include "ReleasePhase.h"
#include "ClearPhaseMath.h"
#include "CoverPhase.h"

float ReleasePhase::GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const {
  (void)settings;
  (void)totalElapsed;
  return ClearPhaseMath::kNormalTimeScale;
}

std::unique_ptr<IClearPhase> ReleasePhase::Next_(const ClearSettings &settings, float carriedSeconds) const {
  (void)settings;
  (void)carriedSeconds; // 覆う段階は時間で進まないので、余りは使わない
  return std::make_unique<CoverPhase>();
}
