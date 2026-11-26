#pragma once
#include "TimedClearPhase.h"

// ボス撃破の演出の段階: Release。次の島が解放される(光の柱、島の色が生き返る)。世界は通常の速さ(Flow で戻り済み)。
// 次へ進む条件: releaseSeconds 秒が過ぎた -> CoverPhase(画面を覆う)
class ReleasePhase : public TimedClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit ReleasePhase(float carriedSeconds) : TimedClearPhase(carriedSeconds) {}

  ClearPhase GetPhase() const override { return ClearPhase::Release; }
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override;

private:
  float Length_(const ClearSettings &settings) const override { return settings.releaseSeconds; }
  std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const override;
};
