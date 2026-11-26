#include "ResultTallyState.h"
#include "IResultFlowActions.h"
#include "ResultAutoPickWaitState.h"
#include "ResultContinueState.h"
#include "ResultMenuState.h"

std::unique_ptr<IResultFlowState> ResultTallyState::Update(IResultFlowActions &actions, float deltaTime) {
  (void)deltaTime;
  if (!actions.IsTallyFinished()) {
    // 集計の途中で決定が押されたら、飛ばす(押した瞬間だけ反応するので、同じ押下で継承へ進むことはない。継承へは、次のフレーム以降の押下で進む)
    if (actions.IsDecidePressed()) {
      actions.SkipTally();
    }
    return nullptr;
  }
  // 今回取った強化が無ければ、選ぶものが無いので、選ばずに継承を確定する(世代だけ進む)
  if (!actions.HasPickCandidates()) {
    actions.CommitInheritance();
    return std::make_unique<ResultMenuState>(ResultMenuState::kOpenNow);
  }
  // 展示中は、一定時間待ってから、自動操作で選ぶ。通常は、案内を出して決定を待つ(結果と評価を、ゆっくり見せるため)
  if (actions.IsExhibition()) {
    return std::make_unique<ResultAutoPickWaitState>();
  }
  return std::make_unique<ResultContinueState>();
}
