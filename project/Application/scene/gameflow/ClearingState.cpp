#include "ClearingState.h"
#include "IGameFlowActions.h"
#include "LeavingState.h"

void ClearingState::Enter(IGameFlowActions &actions) {
  actions.BeginClearDirection();
}

std::unique_ptr<IGameFlowState> ClearingState::Update(IGameFlowActions &actions) {
  // 画面が覆われたら、クリアへ。覆いは、シーンの切り替えが画面を覆う
  if (actions.ConsumeClearCover()) {
    actions.RequestClearScene();
    return std::make_unique<LeavingState>();
  }
  return nullptr;
}
