#include "ResultAutoPickWaitState.h"
#include "ExhibitionMode.h"
#include "IResultFlowActions.h"
#include "ResultPickState.h"

std::unique_ptr<IResultFlowState> ResultAutoPickWaitState::Update(IResultFlowActions &actions, float deltaTime) {
  (void)actions;
  timer_ += deltaTime;
  if (timer_ < ExhibitionMode::kTallyHoldSec) {
    return nullptr;
  }
  // 展示: 自動操作で選ぶ(仮の状態なので、保存されない)
  return std::make_unique<ResultPickState>(ResultPickState::PickMode::AutoPlay);
}
