#pragma once

struct StageStartPose;

// 開幕の演出の状態クラス(IOpeningState)から見た、シーンへの窓口。
//
// 分けた理由: ゲームの進行の窓口(IGameFlowActions)と同じ。状態クラスが GameScene を直接知らないようにし(循環参照を避ける)、
// 状態クラスが使える操作を、ここに書いた最小限に絞る。GameScene が private 継承で実装する。
//
// 役目の分け方: 状態クラスは「いつ、どの段階か」「段階の中のどの瞬間か(透明度、カメラのずれ、加速)」を決める。
// 見た目の反映(カメラと自機を動かす、ステージ名の板の通り道の計算、衝撃の演出、操作のロック)は GameScene が行う。
class IOpeningActions {
public:
  virtual ~IOpeningActions() = default;

  // 画面遷移の覆いが、まだ晴れている途中か(晴れ終わったら false。イントロを終える合図)
  virtual bool IsTransitionRevealing() const = 0;
  // イントロの 1 フレーム分の見た目を反映する(覆いが晴れる進み具合に合わせて、カメラと自機を、進行方向の後ろから進める)
  virtual void ApplyIntroPose() = 0;
  // イントロを終える。カメラと自機を基準の位置へ戻し、ステージ名の板を置き、板を割るまで操作をロックする
  virtual void FinishIntro() = 0;

  // ステージ名の板を、俯瞰のカメラの正面へ置く(俯瞰へ引く・保つ段階)。
  // pose: 今のカメラのずれ(ブーストを含まない)。alpha: 板の透明度(0 〜 1)
  virtual void ShowStageNameInFrontOfView(const StageStartPose &pose, float alpha) = 0;
  // ステージ名の板を、竜の上を越える弧に沿って動かす(戻る段階)。割れたあとは、透明度だけ反映する。
  // returnElapsedSeconds: 戻る段階の始まりからの経過(秒)。alpha: 板の透明度(0 〜 1)
  virtual void MoveStageNameAlongArc(float returnElapsedSeconds, float alpha) = 0;
  // 自機が板に触れた瞬間。板を割り、衝撃を出し、操作のロックを解く
  virtual void BreakStageName() = 0;
  // 開幕の演出を終える。板を消し(割れていなければ、最後の位置へ置く)、操作を必ず戻す
  virtual void FinishOpening() = 0;
};
