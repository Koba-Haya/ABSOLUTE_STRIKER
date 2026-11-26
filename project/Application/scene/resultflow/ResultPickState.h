#pragma once
#include "IResultFlowState.h"

// クリア画面の段階: 継承の選択(今回取った強化から、次の世代へ受け継ぐものを選ぶ)。入ったときに選択を始め、集計は薄く消え始める。
// 選択の入力そのものは、ClearScene が毎フレーム InheritancePicker へ渡す(展示中は自動操作)。
// 次へ進む条件: 選び終わった -> 継承を確定して、ResultMenuState
class ResultPickState : public IResultFlowState {
public:
  // 選び方: 人が選ぶ / 展示の自動操作で選ぶ
  enum class PickMode { Manual, AutoPlay };

  explicit ResultPickState(PickMode mode) : mode_(mode) {}

  void Enter(IResultFlowActions &actions) override;
  std::unique_ptr<IResultFlowState> Update(IResultFlowActions &actions, float deltaTime) override;

private:
  PickMode mode_;
};
