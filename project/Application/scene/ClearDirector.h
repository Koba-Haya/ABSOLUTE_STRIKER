#pragma once

// ボス撃破の演出(ボスの HP が 0 になってから、クリア画面への遷移を要求するまで)の、時間の進行を決めるクラス。
// PlayerDeathDirector と同じ作り: カメラ・自機・ボス・世界は直接動かさない。
// シーンが Update で時間を進め、世界の時間の速さ(GetWorldTimeScale)と、いまのサブフェーズを受け取って反映する。
// 分けた理由: 撃破の見た目(ひび、剥がれ、光の破裂、島の解放)は、サブフェーズごとに別のクラスが担当する。
//             時間の枠をこのクラスに集めておけば、見た目を足すとき、時間の管理をやり直さなくてよい。
//
// 流れ(サブフェーズ。あとのステップの演出は、各サブフェーズに載せる):
//   HitStop : 一瞬、世界が止まる(撃破の衝撃)
//   Crack   : ボスの体にひびが走り、内側から光が漏れる。世界はスローモーション
//   Peel    : 暗い外殻が剥がれて、破片が鱗粉になって舞う
//   Burst   : 光の柱とリングが広がって破裂する
//   Flow    : 光が取り返した島、次の島へ流れる(粒が、次の島へ飛ぶ)。世界の時間は通常へ戻る
//   Release : 次の島が解放される(光の柱、島の色が生き返る)。次の島を解放しない(すでに解放済み、または、次の島が無い)ときは、このサブフェーズは無い(長さ 0)
//   Cover   : クリア画面への遷移を要求する。スキップは、ここへ直接飛ぶ
// 実時間(1 フレームの長さ)で進める。世界だけを、GetWorldTimeScale でゆっくりにする。
//
// State パターン: 今の段階を、段階のクラス(clear/ の HitStopPhase など。基底は IClearPhase)で 1 つ持つ。
// 次の段階、段階の長さ、世界の時間の速さは、段階のクラスが決める。このクラスは、時間を渡して、返された次の段階へ切り替えるだけ。
// GetPhase が返す ClearPhase は「今どの段階か」を外へ見せる名札(見た目のクラスが読む)で、このクラスの遷移には使わない。

#include "clear/ClearPhaseTypes.h"
#include "clear/IClearPhase.h"
#include <memory>

class ClearDirector {
public:
  // 演出を始める(ボスを撃破した瞬間に 1 回呼ぶ)。すでに始まっていれば何もしない
  void Begin(const ClearSettings &settings);

  // 演出を最初の状態へ戻す(シーンの開始時、デバッグ用)
  void Reset();

  // 各サブフェーズが終わる時刻(Begin からの実時間。秒)。カメラのキーフレームなど、時刻で合わせたいものが使う。
  // Release の長さが 0 のとき、Release の終わりは Flow の終わりと同じ。Cover は、最後のサブフェーズの終わりと同じ
  float GetPhaseEndTime(ClearPhase phase) const;

  // 実時間の 1 フレーム分、時間を進める。
  // canSkip が true で、skipRequested が true のとき(かつ、スキップの猶予を過ぎていれば)、Cover へ直接飛ぶ。
  // 展示中は canSkip を false にして渡す
  void Update(float deltaTime, bool canSkip, bool skipRequested);

  bool IsActive() const { return phase_ != nullptr; }
  // 今の段階の名札。始まっていなければ Idle
  ClearPhase GetPhase() const { return phase_ ? phase_->GetPhase() : ClearPhase::Idle; }
  // Begin からの実時間の経過(秒)
  float GetElapsed() const { return elapsed_; }
  // 今のサブフェーズの中での進み具合(0 〜 1)。Cover は 1
  float GetPhaseProgress() const;

  // 世界の時間の速さ(1 で通常)。ヒットストップの間は 0、そのあとスローモーション、Flow で通常へ戻る。
  // 演出が始まっていなければ 1
  float GetWorldTimeScale() const;

  // Cover へ入ったフレームに 1 回だけ true を返す(シーンが、遷移の要求を 1 回だけ出すため)
  bool ConsumeCoverStart();

private:
  // next へ切り替え、Enter を呼ぶ(段階が立てた合図を受け取る)
  void ChangeTo_(std::unique_ptr<IClearPhase> next);

  // 1 フレームに切り替える回数の上限(段階の数。段階は戻らないので、これを超えることはない。万一の無限ループを防ぐ)
  static constexpr int kMaxTransitionsPerFrame = 7;

  ClearSettings settings_;
  std::unique_ptr<IClearPhase> phase_; // 今の段階(始まっていなければ nullptr)
  float elapsed_ = 0.0f;               // Begin からの実時間
  ClearPhaseSignals signals_;          // 段階が立てた合図(Cover に入った)。Consume で 1 回だけ外へ渡す
};
