#pragma once
#include <memory>

class IGameFlowActions;

// ゲームの進行の「状態」の共通の形(State パターンの、状態の基底クラス)。
//
// 状態クラスは「この状態のとき、ゲームのルールとして何を許すか」と「次にどの状態へ進むか」を持つ。
// GameFlow は、今の状態を 1 つ持ち、毎フレーム Update を呼ぶだけ。次の状態は、状態クラス自身が Update の戻り値で決める。
// シーンへは、窓口(IGameFlowActions)経由で触る。GameScene を直接知らないので、依存が一方向になる。
//
// 状態の一覧(遷移):
//   InProgressState(道中)   -> BossBattleState(道中の終点に着いた) / DyingState(自機が死んだ)
//   BossBattleState(ボス戦) -> ClearingState(ボスを倒した)         / DyingState(自機が死んだ)
//   DyingState(死亡演出)    -> LeavingState(演出が画面を覆った。ゲームオーバーへ)
//   ClearingState(撃破演出) -> LeavingState(演出が画面を覆った。クリアへ)
//   LeavingState(終了の要求済み): 進む先は無い(シーンが入れ替わるのを待つ)
//
// 問い合わせ(IsPlayable など)は、シーンの各所が「今の状態で、これをしてよいか」を聞くためのもの。
// 以前は、シーンが GamePhase の値を比べて判断していた。今は、状態クラスが自分の答えを返す(ポリモーフィズム)。
// 既定は、全部 false(何も許さない)。許すものだけを、各状態が上書きする。
class IGameFlowState {
public:
  virtual ~IGameFlowState() = default;

  // この状態に入ったときに 1 回呼ばれる。actions: シーンへの窓口
  virtual void Enter(IGameFlowActions &actions) { (void)actions; }

  // 毎フレーム呼ばれる(Play モードの間だけ)。
  // 戻り値: 次の状態へ進むときは、次の状態。この状態に留まるときは nullptr。
  // 切り替えは、この Update が終わったあとに GameFlow が行う(実行中の状態が、自分自身を消してしまわないようにするため)
  virtual std::unique_ptr<IGameFlowState> Update(IGameFlowActions &actions) = 0;

  // 自機を操作して遊んでいる最中か(道中とボス戦)。成績の計測、マウスの閉じ込め、開発用のショートカットが使う
  virtual bool IsPlayable() const { return false; }
  // 道中を進んでいる最中か(レールカメラを進める、道中の終点を待つ)。展示の自動進行と、ボス戦へ飛ぶショートカットも使う
  virtual bool IsAdvancingRoute() const { return false; }
  // ボス戦の最中か(HUD の目的とバー、ポストエフェクト、ボスを倒すショートカットが使う)
  virtual bool IsBossBattle() const { return false; }
  // ポストエフェクト(HP 1 の赤い枠など)を止めるか。撃破演出の見栄えを損なわないため
  virtual bool SuppressesPostEffects() const { return false; }
};
