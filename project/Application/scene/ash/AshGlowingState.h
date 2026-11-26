#pragma once
#include "IAshState.h"

// ゲームオーバー画面の灰の段階: 灯り。灰は巣のまわりを回り、核がゆっくり脈打つ。集まりは終わっている(進みは 1)。
// 次へ進む条件: 自動で開始の時刻より前へ戻った -> AshDriftingState(漂い)
// 外から集まりを頼まれても、すでに集まりきっているので受けない(基底の既定)。
class AshGlowingState : public IAshState {
public:
  GameOverAshPhase GetPhase() const override { return GameOverAshPhase::Glowing; }
  std::unique_ptr<IAshState> Update(const AshStateContext &context) override;

  float GetGatherProgress(float gatherLengthSeconds) const override {
    (void)gatherLengthSeconds;
    return kGatherCompleted;
  }
  bool IsConverging() const override { return true; }
  bool IsGlowing() const override { return true; }

private:
  static constexpr float kGatherCompleted = 1.0f; // 集まりきった進み
};
