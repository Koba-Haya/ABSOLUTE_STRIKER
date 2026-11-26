#pragma once
#include "IAshState.h"

// ゲームオーバー画面の灰の段階: 集まり。巣へ向かう収束(ばね)と渦で、灰が巣へ集まり、核が少しずつ強くなる。
// 集まりの時計(速さの倍率が掛かる)を、この段階が持つ。
// 次へ進む条件: 集まりきった -> AshGlowingState(灯り)。自動で開始の時刻より前へ戻った -> AshDriftingState(漂い)
// 外から集まりを頼まれても、すでに集まっているので受けない(基底の既定)。
class AshGatheringState : public IAshState {
public:
  // carriedSeconds: 集まりの時計の始まりの値(前の段階から引き継いだ時間)
  explicit AshGatheringState(float carriedSeconds) : gatherClock_(carriedSeconds) {}

  GameOverAshPhase GetPhase() const override { return GameOverAshPhase::Gathering; }
  std::unique_ptr<IAshState> Update(const AshStateContext &context) override;

  float GetGatherProgress(float gatherLengthSeconds) const override;
  bool IsConverging() const override { return true; }

private:
  float gatherClock_ = 0.0f; // 集まりが始まってからの秒(速さの倍率が掛かる)
};
