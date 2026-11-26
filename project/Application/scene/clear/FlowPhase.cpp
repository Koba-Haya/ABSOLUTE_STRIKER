#include "FlowPhase.h"
#include "ClearPhaseMath.h"
#include "CoverPhase.h"
#include "ReleasePhase.h"

float FlowPhase::GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const {
  // Flow に入ってから recoverRampSeconds かけて、スローモーションの速さから、通常の速さへ戻す
  const float slow = ClearPhaseMath::SlowMotionScale(settings, totalElapsed);
  const float recover = ClearPhaseMath::SmoothStep(ClearPhaseMath::Ratio(GetPhaseElapsed(), settings.recoverRampSeconds));
  return slow + (ClearPhaseMath::kNormalTimeScale - slow) * recover;
}

std::unique_ptr<IClearPhase> FlowPhase::Next_(const ClearSettings &settings, float carriedSeconds) const {
  // 次の島を解放しないとき(Release の長さが 0)は、Release を飛ばして、画面を覆う段階へ進む
  if (settings.releaseSeconds > 0.0f) {
    return std::make_unique<ReleasePhase>(carriedSeconds);
  }
  return std::make_unique<CoverPhase>();
}
