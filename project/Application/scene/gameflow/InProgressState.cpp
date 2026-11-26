#include "InProgressState.h"
#include "BossBattleState.h"
#include "DyingState.h"
#include "IGameFlowActions.h"

std::unique_ptr<IGameFlowState> InProgressState::Update(IGameFlowActions &actions) {
  // 死亡を先に調べる(終点に着いたのと同じフレームに死んだときは、ボスを出さずに死亡演出へ進む)
  if (actions.IsPlayerDead()) {
    return std::make_unique<DyingState>();
  }
  // ボス戦への移行は、タイムラインが終点に到達した合図で行う(レールの進行度の値の比較には頼らない)。
  // 到達後は、タイムラインが終端で止まり、レールの進行度は 1.0 のまま保たれるので、ボス戦の間、レールは動かない
  if (actions.HasRouteReachedEnd()) {
    return std::make_unique<BossBattleState>();
  }
  return nullptr;
}
