#pragma once
#include "IClearPhase.h"

// 「決まった長さの時間が過ぎたら、次の段階へ進む」段階の中間クラス(HitStop / Crack / Peel / Burst / Flow / Release)。
//
// 時間で終わる段階に共通の流れ(経過を足す、長さを超えたら余りを次へ渡す、進み具合)を、ここに 1 つだけ書く(Template Method)。
// 派生は、長さ(Length_)と次の段階(Next_)だけを決める。
// 時間で終わらない段階(CoverPhase)は、これを継承せず、IClearPhase を直接継承する。
// 「時間で終わる」という性質を本当に持つ段階だけが継承するので、時間以外の終わり方の段階を、無理に押し込まない。
class TimedClearPhase : public IClearPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit TimedClearPhase(float carriedSeconds) : elapsed_(carriedSeconds) {}

  std::unique_ptr<IClearPhase> Advance(const ClearSettings &settings, float deltaTime) final;
  float GetProgress(const ClearSettings &settings) const final;

protected:
  // この段階に入ってからの経過(秒)。世界の時間の速さを、段階の中の経過で変える段階(FlowPhase)が使う
  float GetPhaseElapsed() const { return elapsed_; }

private:
  // この段階の長さ(秒)
  virtual float Length_(const ClearSettings &settings) const = 0;
  // 次の段階を作る。carriedSeconds: 長さを超えた分(次の段階の経過として渡す)
  virtual std::unique_ptr<IClearPhase> Next_(const ClearSettings &settings, float carriedSeconds) const = 0;

  float elapsed_ = 0.0f; // この段階に入ってからの経過(秒)
};
