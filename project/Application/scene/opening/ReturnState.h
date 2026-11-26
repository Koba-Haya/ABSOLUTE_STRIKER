#pragma once
#include "IOpeningState.h"

// 開幕の演出の状態: 俯瞰から通常の視点へ戻る。世界が動き(道中と同時に進む)、自機がステージ名の板へ向かって飛び、突き破る。
//
// 戻り始めてから breakSeconds で、板を割る(自機が板に触れる。時間で決めるので、毎回同じタイミング)。
// 板を割る時刻へ向けて加速し(ブースト)、割れたあとは、なめらかに通常の速さへ戻る。
// 次へ進む条件: 戻る時間(returnSeconds)が過ぎた -> 演出を終える(IsFinished が true。次の状態は無い)
class ReturnState : public IOpeningState {
public:
  // settings: ステージ開始演出の設定。carriedSeconds: 前の段階から引き継いだ経過(秒)
  ReturnState(const StageStartSettings &settings, float carriedSeconds) : settings_(settings), elapsed_(carriedSeconds) {}

  std::unique_ptr<IOpeningState> Update(IOpeningActions &actions, float deltaTime) override;

  bool IsFinished() const override { return finished_; }
  bool IsWorldRunning() const override { return true; }
  // 俯瞰の度合いを、1 から 0 へ、なめらかに下げる
  StageStartPose GetPose() const override;
  float GetBoost() const override { return StageStartMath::BoostAt(settings_, elapsed_); }

private:
  static constexpr float kStageNameAlpha = 1.0f; // 板は、割れるまで見えたまま(割れたあとの破片の薄れ方は、破片側が決める)

  StageStartSettings settings_;
  float elapsed_ = 0.0f;      // この段階に入ってからの経過(秒)
  bool breakReached_ = false; // 板を割る時刻を過ぎたか(割る合図は 1 回だけ出す)
  bool finished_ = false;     // 戻る時間が過ぎて、演出を終えたか
};
