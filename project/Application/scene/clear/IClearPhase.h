#pragma once
#include "ClearPhaseTypes.h"
#include <memory>

// 段階に入ったときに、ClearDirector へ知らせる合図。段階が立て、ClearDirector が外へ 1 回だけ渡す
struct ClearPhaseSignals {
  bool coverStarted = false; // 画面を覆う段階(Cover)に入った(シーンが、クリアへの遷移を 1 回だけ要求する)
};

// ボス撃破の演出の「段階」の共通の形(State パターンの、状態の基底クラス)。
//
// 段階の一覧(遷移):
//   HitStopPhase -> CrackPhase -> PeelPhase -> BurstPhase -> FlowPhase -> ReleasePhase -> CoverPhase
//   FlowPhase は、次の島を解放しないとき(Release の長さが 0)、ReleasePhase を飛ばして CoverPhase へ進む。
//   スキップは、ClearDirector が、どの段階からでも CoverPhase へ飛ばす(CanSkip が false の段階を除く)。
// 次の段階は、段階のクラス自身が Advance の戻り値で決める。
//
// GetPhase が返す ClearPhase は「今どの段階か」を外へ見せる名札(読み取り専用)。見た目のクラス(BossExplosion など)が読む。
// ClearDirector 自身の遷移には使わない(遷移は段階のクラスが決める)。
class IClearPhase {
public:
  virtual ~IClearPhase() = default;

  // 外へ見せる、今の段階の名札
  virtual ClearPhase GetPhase() const = 0;

  // この段階に入ったときに 1 回呼ばれる。signals: ClearDirector へ知らせる合図(必要な段階だけが立てる)
  virtual void Enter(ClearPhaseSignals &signals) { (void)signals; }

  // 時間を進める。deltaTime: この段階の時間を進める秒数(切り替わった直後の段階には 0。余りは、生成時に受け取っている)。
  // 戻り値: 次の段階へ進むときは、次の段階。留まるときは nullptr
  virtual std::unique_ptr<IClearPhase> Advance(const ClearSettings &settings, float deltaTime) = 0;

  // この段階の中の進み具合(0 〜 1)
  virtual float GetProgress(const ClearSettings &settings) const = 0;

  // 世界の時間の速さ(1 で通常)。totalElapsed: 演出の始まりからの実時間(秒)
  virtual float GetWorldTimeScale(const ClearSettings &settings, float totalElapsed) const = 0;

  // スキップ(画面を覆う段階へ飛ぶ)を受け付けるか。すでに覆っている段階は受け付けない
  virtual bool CanSkip() const { return true; }
};
