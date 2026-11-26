#pragma once
#include "IGameFlowState.h"

// ゲームの進行の状態: 道中(レールに沿って進み、敵を倒す)。シーンの最初の状態。
//
// 次へ進む条件(上から順に調べる):
//   自機が死んだ                 -> DyingState(死亡演出)
//   道中の終点(タイムラインの終わり)に着いた -> BossBattleState(ボス戦)
class InProgressState : public IGameFlowState {
public:
  std::unique_ptr<IGameFlowState> Update(IGameFlowActions &actions) override;

  bool IsPlayable() const override { return true; }
  bool IsAdvancingRoute() const override { return true; }
};
