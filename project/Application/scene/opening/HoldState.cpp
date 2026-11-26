#include "HoldState.h"
#include "IOpeningActions.h"
#include "ReturnState.h"

std::unique_ptr<IOpeningState> HoldState::Update(IOpeningActions &actions, float deltaTime) {
  elapsed_ += deltaTime;
  if (elapsed_ >= settings_.holdSeconds) {
    // 保ち終わった。長さを超えた分は、戻る段階の経過として引き継ぐ
    return std::make_unique<ReturnState>(settings_, elapsed_ - settings_.holdSeconds);
  }
  actions.ShowStageNameInFrontOfView(GetPose(), kStageNameAlpha);
  return nullptr;
}
