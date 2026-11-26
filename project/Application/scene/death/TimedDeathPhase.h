#pragma once
#include "IDeathPhase.h"

// 「決まった長さの時間が過ぎたら、次の段階へ進む」死亡演出の段階の中間クラス(HitStop / Crumble / Fall / Impact)。
//
// 時間で終わる段階に共通の流れ(経過を足す、長さを超えたら余りを次へ渡す、進み具合)を、ここに 1 つだけ書く(Template Method)。
// 派生は、長さ(Length_)と次の段階(Next_)だけを決める。時間で終わらない段階(DeathCoverPhase)は、これを継承しない。
// 撃破演出の TimedClearPhase と同じ形(基底の型と設定の型が違うので、別のクラス。共通化は、あとで見直す項目)。
class TimedDeathPhase : public IDeathPhase {
public:
  // carriedSeconds: 前の段階から引き継いだ経過(秒)
  explicit TimedDeathPhase(float carriedSeconds) : elapsed_(carriedSeconds) {}

  std::unique_ptr<IDeathPhase> Advance(const PlayerDeathSettings &settings, float deltaTime) final;
  float GetProgress(const PlayerDeathSettings &settings) const final;

private:
  // この段階の長さ(秒)
  virtual float Length_(const PlayerDeathSettings &settings) const = 0;
  // 次の段階を作る。carriedSeconds: 長さを超えた分(次の段階の経過として渡す)
  virtual std::unique_ptr<IDeathPhase> Next_(float carriedSeconds) const = 0;

  float elapsed_ = 0.0f; // この段階に入ってからの経過(秒)
};
