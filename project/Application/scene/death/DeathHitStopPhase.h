#pragma once
#include "TimedDeathPhase.h"

// 自機の死亡演出の段階: HitStop。一瞬、世界が止まる(被弾の衝撃)。世界の時間の速さは 0。蝶はまだ崩れていない。
// 次へ進む条件: hitStopSeconds 秒が過ぎた -> DeathCrumblePhase(崩壊)
class DeathHitStopPhase : public TimedDeathPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit DeathHitStopPhase(float carriedSeconds) : TimedDeathPhase(carriedSeconds) {}

  PlayerDeathPhase GetPhase() const override { return PlayerDeathPhase::HitStop; }
  // ヒットストップの間は、世界を止める
  float GetWorldTimeScale(const PlayerDeathSettings &settings, float totalElapsed) const override;
  // 崩壊は、ヒットストップが終わってから始まる
  bool HasCrumbleBegun() const override { return false; }
  // スキップでヒットストップを飛ばしても、崩壊の始まりの合図は 1 回出す(自機をきりもみへ切り替えるため)
  void OnSkipped(DeathPhaseSignals &signals) override { signals.crumbleStarted = true; }

private:
  float Length_(const PlayerDeathSettings &settings) const override { return settings.hitStopSeconds; }
  std::unique_ptr<IDeathPhase> Next_(float carriedSeconds) const override;
};
