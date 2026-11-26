#pragma once
#include "TimedClearPhase.h"

// ボス撃破の演出の段階: Crack。ボスの体にひびが走り、内側から光が漏れる。世界はスローモーション。
// 次へ進む条件: crackSeconds 秒が過ぎた -> PeelPhase(剥がれ)
class CrackPhase : public TimedClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit CrackPhase(float carriedSeconds) : TimedClearPhase(carriedSeconds) {}

  ClearPhase GetPhase() const override { return ClearPhase::Crack; }
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override;

private:
  float Length_(const ClearSettings &settings) const override { return settings.crackSeconds; }
  std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const override;
};
