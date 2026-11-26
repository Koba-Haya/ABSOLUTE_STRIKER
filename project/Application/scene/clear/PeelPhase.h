#pragma once
#include "TimedClearPhase.h"

// ボス撃破の演出の段階: Peel。暗い外殻が剥がれて、破片が鱗粉になって舞う。世界はスローモーション。
// 次へ進む条件: peelSeconds 秒が過ぎた -> BurstPhase(破裂)
class PeelPhase : public TimedClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit PeelPhase(float carriedSeconds) : TimedClearPhase(carriedSeconds) {}

  ClearPhase GetPhase() const override { return ClearPhase::Peel; }
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override;

private:
  float Length_(const ClearSettings &settings) const override { return settings.peelSeconds; }
  std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const override;
};
