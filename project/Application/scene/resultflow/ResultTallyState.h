#pragma once
#include "IResultFlowState.h"

// クリア画面の段階: 結果の集計(評価の内訳、合計、ランクが順に出る)。今回の結果があるときの、最初の段階。
// 集計の途中で決定が押されたら、集計を飛ばす。
// 次へ進む条件: 集計が終わった -> 選ぶ強化が無ければ、選ばずに継承を確定して ResultMenuState。
//               ある場合、展示なら ResultAutoPickWaitState、通常なら ResultContinueState(決定を待つ)
class ResultTallyState : public IResultFlowState {
public:
  std::unique_ptr<IResultFlowState> Update(IResultFlowActions &actions, float deltaTime) override;
};
