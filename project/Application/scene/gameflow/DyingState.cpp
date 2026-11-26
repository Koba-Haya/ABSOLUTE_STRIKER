#include "DyingState.h"
#include "IGameFlowActions.h"
#include "LeavingState.h"

void DyingState::Enter(IGameFlowActions &actions) {
  actions.BeginDeathDirection();
}

std::unique_ptr<IGameFlowState> DyingState::Update(IGameFlowActions &actions) {
  // 画面が覆われたら、ゲームオーバーへ。覆いは、シーンの切り替えが画面を覆う
  if (actions.ConsumeDeathCover()) {
    actions.RequestGameOverScene();
    return std::make_unique<LeavingState>();
  }
  return nullptr;
}
