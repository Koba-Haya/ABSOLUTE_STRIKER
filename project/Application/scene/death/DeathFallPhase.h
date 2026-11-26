#pragma once
#include "TimedDeathPhase.h"

// 自機の死亡演出の段階: Fall。雲海へ向かって落ちる。世界はスローモーション。
// 次へ進む条件: fallSeconds 秒が過ぎた -> DeathImpactPhase(着地)
class DeathFallPhase : public TimedDeathPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit DeathFallPhase(float carriedSeconds) : TimedDeathPhase(carriedSeconds) {}

  PlayerDeathPhase GetPhase() const override { return PlayerDeathPhase::Fall; }

private:
  float Length_(const PlayerDeathSettings &settings) const override { return settings.fallSeconds; }
  std::unique_ptr<IDeathPhase> Next_(float carriedSeconds) const override;
};
