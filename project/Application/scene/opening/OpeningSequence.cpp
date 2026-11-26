#include "OpeningSequence.h"
#include "IOpeningActions.h"
#include "IntroState.h"
#include <utility>

void OpeningSequence::Start(const StageStartSettings &settings) {
  state_ = std::make_unique<IntroState>(settings);
}

void OpeningSequence::Update(IOpeningActions &actions, float deltaTime) {
  if (!state_) {
    return;
  }
  // 状態の Update が終わってから切り替える(Update の途中で、実行中の状態を消さないため)。
  // 切り替えた状態は、時間 0 で進める(このフレームの時間は、余りとして、生成時に渡し済み)
  std::unique_ptr<IOpeningState> next = state_->Update(actions, deltaTime);
  for (int i = 0; next && i < kMaxTransitionsPerFrame; ++i) {
    state_ = std::move(next);
    next = state_->Update(actions, 0.0f);
  }
  if (state_ && state_->IsFinished()) {
    state_.reset();
  }
}
