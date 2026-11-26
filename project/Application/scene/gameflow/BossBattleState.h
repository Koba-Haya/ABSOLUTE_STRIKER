#pragma once
#include "IGameFlowState.h"

// ゲームの進行の状態: ボス戦。入ったときに、ボスを場に出す。
//
// 次へ進む条件(上から順に調べる):
//   自機が死んだ     -> DyingState(死亡演出)
//   ボスを撃破した   -> ClearingState(撃破演出)
class BossBattleState : public IGameFlowState {
public:
  void Enter(IGameFlowActions &actions) override;
  std::unique_ptr<IGameFlowState> Update(IGameFlowActions &actions) override;

  bool IsPlayable() const override { return true; }
  bool IsBossBattle() const override { return true; }
};
