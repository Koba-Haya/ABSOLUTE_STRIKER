#pragma once
#include "IOpeningState.h"

// 開幕の演出の状態: 俯瞰を保つ(世界は止まっている)。ステージ名の板は、カメラの正面に見えたまま。
//
// 次へ進む条件: 保つ時間(holdSeconds)が過ぎた -> ReturnState(戻りながら板を突き破る)
class HoldState : public IOpeningState {
public:
  // settings: ステージ開始演出の設定。carriedSeconds: 前の段階から引き継いだ経過(秒)
  HoldState(const StageStartSettings &settings, float carriedSeconds) : settings_(settings), elapsed_(carriedSeconds) {}

  std::unique_ptr<IOpeningState> Update(IOpeningActions &actions, float deltaTime) override;

  // 俯瞰の度合いは最大のまま
  StageStartPose GetPose() const override { return StageStartMath::MakePose(settings_, kFullBlend); }

private:
  static constexpr float kFullBlend = 1.0f;          // 俯瞰の度合いの最大
  static constexpr float kStageNameAlpha = 1.0f;     // 板は見えたまま

  StageStartSettings settings_;
  float elapsed_ = 0.0f; // この段階に入ってからの経過(秒)
};
