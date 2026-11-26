#include "ResultPickState.h"
#include "IResultFlowActions.h"
#include "ResultMenuState.h"

void ResultPickState::Enter(IResultFlowActions &actions) {
  actions.BeginPick(mode_ == PickMode::AutoPlay);
}

std::unique_ptr<IResultFlowState> ResultPickState::Update(IResultFlowActions &actions, float deltaTime) {
  (void)deltaTime;
  if (!actions.IsPickDone()) {
    return nullptr;
  }
  actions.CommitInheritance();
  return std::make_unique<ResultMenuState>(ResultMenuState::kOpenNow);
}
