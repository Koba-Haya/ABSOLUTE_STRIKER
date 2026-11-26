#pragma once
#include "TimedClearPhase.h"

// ボス撃破の演出の段階: HitStop。一瞬、世界が止まる(撃破の衝撃)。世界の時間の速さは 0。
// 次へ進む条件: hitStopSeconds 秒が過ぎた -> CrackPhase(ひび)
class HitStopPhase : public TimedClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit HitStopPhase(float carriedSeconds) : TimedClearPhase(carriedSeconds) {}

  ClearPhase GetPhase() const override { return ClearPhase::HitStop; }
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override;

private:
  float Length_(const ClearSettings &settings) const override { return settings.hitStopSeconds; }
  std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const override;
};
