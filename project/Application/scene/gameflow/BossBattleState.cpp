#include "BossBattleState.h"
#include "ClearingState.h"
#include "DyingState.h"
#include "IGameFlowActions.h"

void BossBattleState::Enter(IGameFlowActions &actions) {
  // ボス戦に入る = ボスが現れる。ボスを出す場所を、この状態の入り口 1 か所にまとめる
  actions.SpawnBoss();
}

std::unique_ptr<IGameFlowState> BossBattleState::Update(IGameFlowActions &actions) {
  // 死亡を先に調べる(相打ちのときは、死亡演出を優先する。以前の CheckGameEnd_ と同じ順)
  if (actions.IsPlayerDead()) {
    return std::make_unique<DyingState>();
  }
  if (actions.IsBossDefeated()) {
    return std::make_unique<ClearingState>();
  }
  return nullptr;
}
