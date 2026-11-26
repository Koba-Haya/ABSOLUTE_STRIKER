#pragma once
#include "TimedClearPhase.h"

// ボス撃破の演出の段階: Flow。光が、取り返した島から次の島へ流れる(粒が次の島へ飛ぶ)。世界の時間は、スローから通常へなめらかに戻る。
// 次へ進む条件: flowSeconds 秒が過ぎた -> 次の島を解放するとき(releaseSeconds が 0 より大きい)は ReleasePhase、しないときは CoverPhase
class FlowPhase : public TimedClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit FlowPhase(float carriedSeconds) : TimedClearPhase(carriedSeconds) {}

  ClearPhase GetPhase() const override { return ClearPhase::Flow; }
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override;

private:
  float Length_(const ClearSettings &settings) const override { return settings.flowSeconds; }
  std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const override;
};
