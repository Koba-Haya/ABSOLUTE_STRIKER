#pragma once
#include "IGameFlowState.h"

// ゲームの進行の状態: ボス撃破の演出(ボスを倒してから、演出が画面を覆うまで)。入ったときに、撃破演出を始める。
//
// 次へ進む条件:
//   演出が画面を覆う段階(Cover)へ入った -> クリアへの切り替えを要求して、LeavingState(終了の要求済み)
// 演出の間は、ポストエフェクト(HP 1 の赤い枠など)を止める。演出そのものは、GameScene が毎フレーム進める(DyingState と同じ理由)。
class ClearingState : public IGameFlowState {
public:
  void Enter(IGameFlowActions &actions) override;
  std::unique_ptr<IGameFlowState> Update(IGameFlowActions &actions) override;

  bool SuppressesPostEffects() const override { return true; }
};
