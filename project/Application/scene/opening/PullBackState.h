#pragma once
#include "IOpeningState.h"

// 開幕の演出の状態: 通常の視点から俯瞰へ引く(世界は止まっている)。ステージ名の板が、カメラの正面に現れる。
//
// 次へ進む条件: 引く時間(pullBackSeconds)が過ぎた -> HoldState(俯瞰を保つ)
class PullBackState : public IOpeningState {
public:
  // settings: ステージ開始演出の設定。carriedSeconds: 前の段階から引き継いだ経過(秒)
  PullBackState(const StageStartSettings &settings, float carriedSeconds) : settings_(settings), elapsed_(carriedSeconds) {}

  std::unique_ptr<IOpeningState> Update(IOpeningActions &actions, float deltaTime) override;

  // 俯瞰の度合いを、0 から 1 へ、なめらかに上げる
  StageStartPose GetPose() const override;

private:
  // この段階の中の進み具合(0 〜 1)。板の透明度にもそのまま使う(引く間に現れる)
  float Progress_() const { return StageStartMath::Progress(elapsed_, settings_.pullBackSeconds); }

  StageStartSettings settings_;
  float elapsed_ = 0.0f; // この段階に入ってからの経過(秒)
};
