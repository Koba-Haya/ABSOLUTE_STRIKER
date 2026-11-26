#pragma once
#include "IDeathPhase.h"

// 自機の死亡演出の段階: Cover(最後の段階)。霧と水滴で覆い、ゲームオーバー画面への遷移を要求する。画面は、遷移の覆いが覆う。
//
// 時間で終わらない(シーンが入れ替わるまで続く)ので、TimedDeathPhase を継承せず、IDeathPhase を直接継承する。
// 入ったときに「覆う段階に入った」合図を立てる(シーンが、ゲームオーバーへの遷移を 1 回だけ要求する)。
// 進む先は無い。すでに覆っているので、スキップも受け付けない。世界の時間は、スローモーションのまま(基底の既定)。
// 処理が短いので、ヘッダだけで完結させている(.cpp を作らない)。
class DeathCoverPhase : public IDeathPhase {
public:
  PlayerDeathPhase GetPhase() const override { return PlayerDeathPhase::Cover; }

  void Enter(DeathPhaseSignals &signals) override { signals.coverStarted = true; }

  std::unique_ptr<IDeathPhase> Advance(const PlayerDeathSettings &settings, float deltaTime) override {
    (void)settings;
    (void)deltaTime;
    return nullptr;
  }

  // 覆う段階は、入った時点で終わっている扱い(1)
  float GetProgress(const PlayerDeathSettings &settings) const override {
    (void)settings;
    return kCompletedProgress;
  }

  bool CanSkip() const override { return false; }

private:
  static constexpr float kCompletedProgress = 1.0f;
};
