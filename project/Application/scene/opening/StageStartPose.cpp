#include "StageStartPose.h"

#include "Method.h"

#include <algorithm>

namespace StageStartMath {

float Progress(float elapsed, float duration) {
  if (duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(elapsed / duration, 0.0f, 1.0f);
}

float SmoothStep(float t) {
  return t * t * (3.0f - 2.0f * t);
}

StageStartPose MakePose(const StageStartSettings &settings, float blend) {
  StageStartPose pose;
  pose.backOffset = settings.pullBackDistance * blend;
  pose.riseOffset = settings.riseHeight * blend;
  pose.lookAheadOffset = settings.lookAheadDistance * blend;
  pose.fovScale = 1.0f + (settings.fovScale - 1.0f) * blend;
  return pose;
}

void ApplyPose(const StageStartPose &pose, const Vector3 &eye, const Vector3 &target, Vector3 &outEye, Vector3 &outTarget) {
  const Vector3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
  outEye = {eye.x - forward.x * pose.backOffset, eye.y - forward.y * pose.backOffset + pose.riseOffset,
            eye.z - forward.z * pose.backOffset};
  outTarget = {target.x + forward.x * pose.lookAheadOffset, target.y + forward.y * pose.lookAheadOffset,
               target.z + forward.z * pose.lookAheadOffset};
}

float BoostAt(const StageStartSettings &settings, float returnElapsedSeconds) {
  const float breakSeconds = settings.breakSeconds;
  if (returnElapsedSeconds <= 0.0f || breakSeconds <= 0.0f) {
    return 0.0f;
  }
  if (returnElapsedSeconds <= breakSeconds) {
    // 板を割る時刻へ向けて、だんだん速く上がる(2 次曲線)
    const float t = returnElapsedSeconds / breakSeconds;
    return t * t;
  }
  // 割れたあとは、「戻る」段階の終わりへ向けて、なめらかに 0 へ戻る
  const float decaySeconds = settings.returnSeconds - breakSeconds;
  if (decaySeconds <= 0.0f) {
    return 0.0f;
  }
  return 1.0f - SmoothStep(Progress(returnElapsedSeconds - breakSeconds, decaySeconds));
}

} // namespace StageStartMath
