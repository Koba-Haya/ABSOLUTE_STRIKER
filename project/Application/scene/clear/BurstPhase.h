#pragma once
#include "TimedClearPhase.h"

// ボス撃破の演出の段階: Burst。光の柱とリングが広がって破裂する。世界はスローモーション。
// 次へ進む条件: burstSeconds 秒が過ぎた -> FlowPhase(島へ流れる)
class BurstPhase : public TimedClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit BurstPhase(float carriedSeconds) : TimedClearPhase(carriedSeconds) {}

  ClearPhase GetPhase() const override { return ClearPhase::Burst; }
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override;

private:
  float Length_(const ClearSettings &settings) const override { return settings.burstSeconds; }
  std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const override;
};
