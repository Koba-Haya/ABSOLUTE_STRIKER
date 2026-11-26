#pragma once
#include "IAshState.h"

// ゲームオーバー画面の灰の段階: 漂い(最初の段階)。カメラの前方の広い範囲に、灰がゆっくり漂う。核は灯っていない。
// 次へ進む条件: 自動で開始の時刻を過ぎた、または外から集まりを頼まれた -> AshGatheringState(集まり)
class AshDriftingState : public IAshState {
public:
  GameOverAshPhase GetPhase() const override { return GameOverAshPhase::Drifting; }
  std::unique_ptr<IAshState> Update(const AshStateContext &context) override;
  std::unique_ptr<IAshState> OnGatherRequested() override;
};
