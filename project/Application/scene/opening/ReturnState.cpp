#include "ReturnState.h"
#include "IOpeningActions.h"

std::unique_ptr<IOpeningState> ReturnState::Update(IOpeningActions &actions, float deltaTime) {
  elapsed_ += deltaTime;
  if (elapsed_ >= settings_.returnSeconds) {
    // 戻り終わった。演出を終える(板を消し、操作を必ず戻す)。最後の段階なので、次の状態は無い
    finished_ = true;
    actions.FinishOpening();
    return nullptr;
  }
  // 板を割る時刻を過ぎたら、1 回だけ割る
  if (!breakReached_ && elapsed_ >= settings_.breakSeconds) {
    breakReached_ = true;
    actions.BreakStageName();
  }
  actions.MoveStageNameAlongArc(elapsed_, kStageNameAlpha);
  return nullptr;
}

StageStartPose ReturnState::GetPose() const {
  const float progress = StageStartMath::Progress(elapsed_, settings_.returnSeconds);
  return StageStartMath::MakePose(settings_, 1.0f - StageStartMath::SmoothStep(progress));
}
