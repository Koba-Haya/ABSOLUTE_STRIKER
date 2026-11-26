#pragma once
#include "IGameFlowState.h"
#include <memory>

class IGameFlowActions;

// ゲームの進行(道中 -> ボス戦 -> 撃破演出 / 死亡演出 -> 終了)を持つクラス(State パターンの、状態を持つ側)。
//
// 役目: 今の状態を 1 つ持ち、毎フレーム Update を呼び、状態が返した次の状態へ切り替える。
// 次にどこへ進むかは、ここでは決めない(状態クラス自身が決める)。
// 分けた理由: GameScene は描画・演出・エディタなど多くを持つので、「状態を持って切り替える」役を別のクラスにして、
// GameScene の責務を増やさない。GameScene は、これを 1 つ持ち、Update を呼び、問い合わせるだけ。
class GameFlow {
public:
  // 最初の状態から始める(今の状態があれば、置き換える)。first の Enter を呼ぶ。
  // first: 最初の状態(nullptr は不可)。actions: シーンへの窓口
  void Start(std::unique_ptr<IGameFlowState> first, IGameFlowActions &actions);

  // 今の状態を 1 回進め、次の状態が返されたら切り替える(次の状態の Enter を呼ぶ)。
  // 切り替えは 1 フレームに 1 回まで(状態が連鎖して、同じフレームで何度も切り替わらないようにする)。
  // Start の前に呼んだときは、何もしない
  void Update(IGameFlowActions &actions);

  // 今の状態への問い合わせ。意味は IGameFlowState の同名の関数を参照。Start の前は、全部 false
  bool IsPlayable() const { return state_ && state_->IsPlayable(); }
  bool IsAdvancingRoute() const { return state_ && state_->IsAdvancingRoute(); }
  bool IsBossBattle() const { return state_ && state_->IsBossBattle(); }
  bool SuppressesPostEffects() const { return state_ && state_->SuppressesPostEffects(); }

private:
  // next へ切り替え、Enter を呼ぶ
  void ChangeTo_(std::unique_ptr<IGameFlowState> next, IGameFlowActions &actions);

  std::unique_ptr<IGameFlowState> state_; // 今の状態(Start の前は nullptr)
};
