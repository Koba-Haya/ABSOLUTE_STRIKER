#pragma once
#include "TimedDeathPhase.h"

// 自機の死亡演出の段階: Crumble。蝶が崩れ始める。世界はスローモーション、自機はきりもみで落ち始める。
// 次へ進む条件: crumbleSeconds 秒が過ぎた -> DeathFallPhase(落下)
class DeathCrumblePhase : public TimedDeathPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit DeathCrumblePhase(float carriedSeconds) : TimedDeathPhase(carriedSeconds) {}

  PlayerDeathPhase GetPhase() const override { return PlayerDeathPhase::Crumble; }
  // 入った瞬間に、崩壊の始まりの合図を立てる(シーンが、自機をきりもみへ切り替える)
  void Enter(DeathPhaseSignals &signals) override { signals.crumbleStarted = true; }

private:
  float Length_(const PlayerDeathSettings &settings) const override { return settings.crumbleSeconds; }
  std::unique_ptr<IDeathPhase> Next_(float carriedSeconds) const override;
};
