#pragma once
#include "TimedDeathPhase.h"

// 自機の死亡演出の段階: Impact。雲海への着地。シーンが、この段階に入ったのを見て、雲の反応を起こす。
// 次へ進む条件: impactSeconds 秒が過ぎた -> DeathCoverPhase(画面を覆う)
class DeathImpactPhase : public TimedDeathPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit DeathImpactPhase(float carriedSeconds) : TimedDeathPhase(carriedSeconds) {}

  PlayerDeathPhase GetPhase() const override { return PlayerDeathPhase::Impact; }

private:
  float Length_(const PlayerDeathSettings &settings) const override { return settings.impactSeconds; }
  std::unique_ptr<IDeathPhase> Next_(float carriedSeconds) const override;
};
