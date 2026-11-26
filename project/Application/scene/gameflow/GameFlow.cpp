#include "GameFlow.h"
#include "IGameFlowActions.h"
#include <utility>

void GameFlow::Start(std::unique_ptr<IGameFlowState> first, IGameFlowActions &actions) {
  ChangeTo_(std::move(first), actions);
}

void GameFlow::Update(IGameFlowActions &actions) {
  if (!state_) {
    return;
  }
  // 状態の Update が終わってから切り替える(Update の途中で、実行中の状態を消さないため)
  std::unique_ptr<IGameFlowState> next = state_->Update(actions);
  if (next) {
    ChangeTo_(std::move(next), actions);
  }
}

void GameFlow::ChangeTo_(std::unique_ptr<IGameFlowState> next, IGameFlowActions &actions) {
  state_ = std::move(next);
  if (state_) {
    state_->Enter(actions);
  }
}
