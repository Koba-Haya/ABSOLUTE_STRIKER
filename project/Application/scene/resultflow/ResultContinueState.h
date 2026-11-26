#pragma once
#include "IResultFlowState.h"

// クリア画面の段階: 継承へ進む決定を待つ(通常のとき)。「決定ボタンかクリックで 継承へ」の案内を出す。
// 次へ進む条件: 決定が押された -> 案内を消して、継承の選択を始め、ResultPickState
class ResultContinueState : public IResultFlowState {
public:
  void Enter(IResultFlowActions &actions) override;
  std::unique_ptr<IResultFlowState> Update(IResultFlowActions &actions, float deltaTime) override;
};
