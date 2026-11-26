#pragma once
#include "IGameFlowState.h"

// ゲームの進行の状態: 死亡演出(自機の HP が 0 になってから、演出が画面を覆うまで)。入ったときに、死亡演出を始める。
//
// 次へ進む条件:
//   演出が画面を覆う段階(Cover)へ入った -> ゲームオーバーへの切り替えを要求して、LeavingState(終了の要求済み)
// 演出そのもの(時間を進める、スキップ、世界の時間の速さ)は、GameScene が毎フレーム進める。
// 演出は、切り替えの要求のあとも、画面が覆われる間、動き続けるので、状態とは寿命が違う。
class DyingState : public IGameFlowState {
public:
  void Enter(IGameFlowActions &actions) override;
  std::unique_ptr<IGameFlowState> Update(IGameFlowActions &actions) override;
};
