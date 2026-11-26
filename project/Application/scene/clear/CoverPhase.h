#pragma once
#include "ClearPhaseMath.h"
#include "IClearPhase.h"

// ボス撃破の演出の段階: Cover(最後の段階)。クリア画面への遷移を要求する。画面は、遷移の覆いが覆う。
//
// 時間で終わらない(シーンが入れ替わるまで続く)ので、TimedClearPhase を継承せず、IClearPhase を直接継承する。
// 入ったときに「覆う段階に入った」合図を立てる(シーンが、クリアへの遷移を 1 回だけ要求する)。
// 進む先は無い。すでに覆っているので、スキップも受け付けない。
// 処理が短いので、ヘッダだけで完結させている(.cpp を作らない)。
class CoverPhase : public IClearPhase {
public:
  ClearPhase GetPhase() const override { return ClearPhase::Cover; }

  void Enter(ClearPhaseSignals &signals) override { signals.coverStarted = true; }

  std::unique_ptr<IClearPhase> Advance(const ClearSettings &settings, float deltaTime) override {
    (void)settings;
    (void)deltaTime;
    return nullptr;
  }

  // 覆う段階は、入った時点で終わっている扱い(1)
  float GetProgress(const ClearSettings &settings) const override {
    (void)settings;
    return kCompletedProgress;
  }

  // 覆ったあと(スキップを含む)は、通常の速さ
  float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const override {
    (void)settings;
    (void)totalElapsed;
    return ClearPhaseMath::kNormalTimeScale;
  }

  bool CanSkip() const override { return false; }

private:
  static constexpr float kCompletedProgress = 1.0f;
};
