#pragma once

// ゲームの進行の状態クラス(IGameFlowState)から見た、シーンへの窓口。
//
// 分けた理由: 状態クラスが GameScene を直接触ると、GameScene(状態を持つ)と状態クラス(GameScene を使う)が
// お互いを知る循環参照になり、状態クラスのために GameScene の private を公開する必要も出る。
// 状態クラスには、このインターフェースだけを見せ、GameScene が実装する(ボスの IBossActions と同じ形)。
// 状態クラスが使える操作を、ここに書いた最小限に絞る役目もある。
//
// 操作は「問い合わせ(Is / Has)」「演出を始める(Begin)」「演出が終わりの合図を出したか(Consume)」「次のシーンを要求する(Request)」の 4 種類。
// 演出の中身(演出クラスの設定、カメラ、自機の見た目など)は GameScene が持つ。状態クラスは「いつ始めるか」「次へ進むか」だけを決める。
class IGameFlowActions {
public:
  virtual ~IGameFlowActions() = default;

  // 自機の HP が 0 になったか。自機がいないときは false
  virtual bool IsPlayerDead() const = 0;
  // 道中の終点(タイムラインの終わり)に着いたか
  virtual bool HasRouteReachedEnd() const = 0;
  // 場のボスが撃破されたか。撃破されたボスは、撃破演出が姿を使うので、消えずに残る(BossComponent::IsActive が false)。
  // そのため「ボスがいない」も「全員が撃破済み」も、撃破とみなす
  virtual bool IsBossDefeated() const = 0;

  // ボスを場に出す(ボス戦の始まりに 1 回呼ぶ)
  virtual void SpawnBoss() = 0;

  // 死亡演出を始める。操作・発射・ロックオンを止め、演出クラスに、死亡の瞬間の自機の状態を渡す
  virtual void BeginDeathDirection() = 0;
  // 死亡演出が、画面を覆う段階(Cover)へ入ったか。true を返すのは 1 回だけ(合図を消費する)。
  // デバッグで「Cover で遷移しない」設定のときは、合図を消費して false を返す
  virtual bool ConsumeDeathCover() = 0;

  // ボス撃破の演出を始める。操作をロックして自機を無敵にし、成績の評価、次の島の解放、演出クラスの開始を行う
  virtual void BeginClearDirection() = 0;
  // 撃破演出が、画面を覆う段階(Cover)へ入ったか。true を返すのは 1 回だけ(合図を消費する)
  virtual bool ConsumeClearCover() = 0;

  // ゲームオーバーのシーンへの切り替えを要求する
  virtual void RequestGameOverScene() = 0;
  // クリアのシーンへの切り替えを要求する(今回のプレイの結果を渡す)
  virtual void RequestClearScene() = 0;
};
