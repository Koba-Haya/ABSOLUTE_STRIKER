#include "ResultContinueState.h"
#include "IResultFlowActions.h"
#include "ResultPickState.h"

void ResultContinueState::Enter(IResultFlowActions &actions) {
  actions.ShowContinuePrompt();
}

std::unique_ptr<IResultFlowState> ResultContinueState::Update(IResultFlowActions &actions, float deltaTime) {
  (void)deltaTime;
  if (!actions.IsDecidePressed()) {
    return nullptr;
  }
  actions.HideContinuePrompt();
  return std::make_unique<ResultPickState>(ResultPickState::PickMode::Manual);
}
