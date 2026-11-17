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
//   Flow    : 光が取り返した島、次の島へ流れる(次の島の解放)。世界の時間は通常へ戻る
//   Cover   : クリア画面への遷移を要求する。スキップは、ここへ直接飛ぶ
// 実時間(1 フレームの長さ)で進める。世界だけを、GetWorldTimeScale でゆっくりにする。

// 演出の長さ・動きの設定。ステージ(ボス)ごとに変わりうる値なので、コードに埋め込まず、まとめて持つ
namespace ClearDefaults {
// 各サブフェーズの長さ(秒)。Cover は、遷移の覆いが担うので、ここでは長さを持たない。
// 全体: 各サブフェーズの合計 + 遷移の覆いで、6 秒以内にする
constexpr float kHitStopSeconds = 0.15f;
constexpr float kCrackSeconds = 0.8f;
constexpr float kPeelSeconds = 1.0f;
constexpr float kBurstSeconds = 0.5f;
constexpr float kFlowSeconds = 1.5f;

// ヒットストップのあとの、世界の時間の速さ(1 で通常、0 で停止)と、そこへ向かう時間(秒)
constexpr float kSlowMotionScale = 0.3f;
constexpr float kSlowMotionRampSeconds = 0.2f;
// Flow へ入ってから、世界の時間が通常の速さ(1)へ戻るまでの時間(秒)
constexpr float kRecoverRampSeconds = 0.5f;

// スキップを受け付け始めるまでの猶予(秒)。0 で、すぐ受け付ける
constexpr float kSkipGuardSeconds = 0.5f;
} // namespace ClearDefaults

struct ClearSettings {
  float hitStopSeconds = ClearDefaults::kHitStopSeconds;
  float crackSeconds = ClearDefaults::kCrackSeconds;
  float peelSeconds = ClearDefaults::kPeelSeconds;
  float burstSeconds = ClearDefaults::kBurstSeconds;
  float flowSeconds = ClearDefaults::kFlowSeconds;
  float slowMotionScale = ClearDefaults::kSlowMotionScale;
  float slowMotionRampSeconds = ClearDefaults::kSlowMotionRampSeconds;
  float recoverRampSeconds = ClearDefaults::kRecoverRampSeconds;
  float skipGuardSeconds = ClearDefaults::kSkipGuardSeconds;
};

enum class ClearPhase {
  Idle, // 始まっていない
  HitStop,
  Crack,
  Peel,
  Burst,
  Flow,
  Cover
};

class ClearDirector {
public:
  // 演出を始める(ボスを撃破した瞬間に 1 回呼ぶ)。すでに始まっていれば何もしない
  void Begin(const ClearSettings &settings);

  // 演出を最初の状態へ戻す(シーンの開始時、デバッグ用)
  void Reset();

  // 各サブフェーズが終わる時刻(Begin からの実時間。秒)。カメラのキーフレームなど、時刻で合わせたいものが使う。
  // Cover は、Flow の終わりと同じ
  float GetPhaseEndTime(ClearPhase phase) const;

  // 実時間の 1 フレーム分、時間を進める。
  // canSkip が true で、skipRequested が true のとき(かつ、スキップの猶予を過ぎていれば)、Cover へ直接飛ぶ。
  // 展示中は canSkip を false にして渡す
  void Update(float deltaTime, bool canSkip, bool skipRequested);

  bool IsActive() const { return phase_ != ClearPhase::Idle; }
  ClearPhase GetPhase() const { return phase_; }
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
  void EnterPhase_(ClearPhase phase);
  // サブフェーズの長さ(秒)。Idle / Cover は、時間で終わらないので 0
  float PhaseLength_(ClearPhase phase) const;

  ClearSettings settings_;
  ClearPhase phase_ = ClearPhase::Idle;
  float elapsed_ = 0.0f;      // Begin からの実時間
  float phaseElapsed_ = 0.0f; // 今のサブフェーズに入ってからの実時間
  bool coverStartPending_ = false;
};
