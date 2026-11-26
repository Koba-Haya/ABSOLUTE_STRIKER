#pragma once
#include "PlayerDeathTypes.h"
#include <memory>

// 段階に入ったとき(またはスキップされたとき)に、PlayerDeathDirector へ知らせる合図。段階が立て、PlayerDeathDirector が外へ 1 回だけ渡す
struct DeathPhaseSignals {
  bool crumbleStarted = false; // 蝶の崩壊が始まった(シーンが、自機をきりもみへ切り替える)
  bool coverStarted = false;   // 画面を覆う段階(Cover)に入った(シーンが、ゲームオーバーへの遷移を 1 回だけ要求する)
};

// 自機の死亡演出の「段階」の共通の形(State パターンの、状態の基底クラス)。
//
// 段階の一覧(遷移):
//   DeathHitStopPhase -> DeathCrumblePhase -> DeathFallPhase -> DeathImpactPhase -> DeathCoverPhase
//   スキップは、PlayerDeathDirector が、どの段階からでも DeathCoverPhase へ飛ばす(CanSkip が false の段階を除く)。
//   飛ばされる段階は OnSkipped で、飛ばしても出すべき合図を立てる(ヒットストップを飛ばしても、崩壊の始まりの合図は 1 回出す)。
// 次の段階は、段階のクラス自身が Advance の戻り値で決める。
//
// GetPhase が返す PlayerDeathPhase は「今どの段階か」を外へ見せる名札(読み取り専用)。GameScene が読む(着地の瞬間、調整窓の表示)。
// PlayerDeathDirector 自身の判断には使わない(段階ごとの違いは、下の問い合わせで答える)。
//
// 自機の落下の位置・速度、崩壊の進み具合、ディゾルブは、段階をまたいで続く「演出の始まりからの時間」の関数なので、
// 段階のクラスではなく PlayerDeathDirector が計算する。段階が答えるのは「崩壊が始まっているか」だけ。
class IDeathPhase {
public:
  virtual ~IDeathPhase() = default;

  // 外へ見せる、今の段階の名札
  virtual PlayerDeathPhase GetPhase() const = 0;

  // この段階に入ったときに 1 回呼ばれる。signals: PlayerDeathDirector へ知らせる合図(必要な段階だけが立てる)
  virtual void Enter(DeathPhaseSignals &signals) { (void)signals; }

  // スキップで、この段階から覆う段階へ飛ばされるときに呼ばれる。飛ばしても出すべき合図があれば立てる
  virtual void OnSkipped(DeathPhaseSignals &signals) { (void)signals; }

  // 時間を進める。deltaTime: この段階の時間を進める秒数(切り替わった直後の段階には 0。余りは、生成時に受け取っている)。
  // 戻り値: 次の段階へ進むときは、次の段階。留まるときは nullptr
  virtual std::unique_ptr<IDeathPhase> Advance(const PlayerDeathSettings &settings, float deltaTime) = 0;

  // この段階の中の進み具合(0 〜 1)
  virtual float GetProgress(const PlayerDeathSettings &settings) const = 0;

  // 世界の時間の速さ(1 で通常)。既定は、ヒットストップのあとのスローモーション。totalElapsed: 演出の始まりからの実時間(秒)
  virtual float GetWorldTimeScale(const PlayerDeathSettings &settings, float totalElapsed) const;

  // 蝶の崩壊が始まっているか(崩壊の進み具合とディゾルブを出すか)。ヒットストップの間だけ false
  virtual bool HasCrumbleBegun() const { return true; }

  // スキップ(画面を覆う段階へ飛ぶ)を受け付けるか。すでに覆っている段階は受け付けない
  virtual bool CanSkip() const { return true; }
};
