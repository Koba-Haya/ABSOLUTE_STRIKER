#include "PullBackState.h"
#include "HoldState.h"
#include "IOpeningActions.h"

std::unique_ptr<IOpeningState> PullBackState::Update(IOpeningActions &actions, float deltaTime) {
  elapsed_ += deltaTime;
  if (elapsed_ >= settings_.pullBackSeconds) {
    // 引き終わった。長さを超えた分は、保つ段階の経過として引き継ぐ
    return std::make_unique<HoldState>(settings_, elapsed_ - settings_.pullBackSeconds);
  }
  actions.ShowStageNameInFrontOfView(GetPose(), Progress_());
  return nullptr;
}

StageStartPose PullBackState::GetPose() const {
  return StageStartMath::MakePose(settings_, StageStartMath::SmoothStep(Progress_()));
}
