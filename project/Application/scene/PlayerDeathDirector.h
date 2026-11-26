#pragma once
#include "Vector.h"

// 自機の死亡演出(HP が 0 になってから、ゲームオーバー画面への遷移を要求するまで)の、時間の進行と自機の位置・向きを決めるクラス。
// 開幕の演出(OpeningSequence)と同じ作り: カメラ・自機・世界は直接動かさない。シーンが Update で時間を進め、
// 世界の時間の速さ(GetWorldTimeScale)と自機の位置・向き(GetPlayerPose)を受け取って反映する。
//
// 流れ(サブフェーズ。あとのステップの演出は、各サブフェーズに載せる):
//   HitStop : 一瞬、世界が止まる(被弾の衝撃)
//   Crumble : 蝶が崩れる(崩壊の演出はステップ 3)。世界はスローモーション、自機はきりもみで落ち始める
//   Fall    : 落下(死亡カメラはステップ 4)
//   Impact  : 雲海への着地(ステップ 5)
//   Cover   : 霧と水滴で覆う。ここで遷移を要求し、遷移の覆いが画面を覆う(遷移そのものは SceneManager が進める)。
//             スキップは、ここへ直接飛ぶ
// 実時間(1 フレームの長さ)で進める。世界だけを、GetWorldTimeScale でゆっくりにする。
//
// State パターン: 今の段階を、段階のクラス(death/ の DeathHitStopPhase など。基底は IDeathPhase)で 1 つ持つ。
// 次の段階、段階の長さ、世界の時間の速さ、崩壊が始まっているか、スキップで出す合図は、段階のクラスが決める。
// 自機の落下の動き・崩壊の進み具合・ディゾルブは、段階をまたいで続く時間の関数なので、このクラスが計算する。
// GetPhase が返す PlayerDeathPhase は「今どの段階か」を外へ見せる名札で、このクラスの判断には使わない。

#include "death/PlayerDeathTypes.h"
#include "death/IDeathPhase.h"
#include <memory>

// 死亡の瞬間の自機の状態(位置・向き・進んでいた向きと速さ)。落下の動きの出発点になる
struct PlayerDeathStart {
  Vector3 position{0.0f, 0.0f, 0.0f};
  Vector3 rotate{0.0f, 0.0f, 0.0f};  // Transform の回転(ピッチ・ヨー・ロール)
  Vector3 forward{0.0f, 0.0f, 1.0f}; // 進んでいた向き(正規化済み)
  float railSpeed = 0.0f;            // レールの進む速さ(ユニット / 秒)
};

// 今の瞬間の、自機の位置と向き
struct PlayerDeathPose {
  Vector3 position{0.0f, 0.0f, 0.0f};
  Vector3 rotate{0.0f, 0.0f, 0.0f};
};

// 今の瞬間の、蝶のディゾルブの状態(ModelComponent / DissolveComponent へ渡す)
struct PlayerDeathDissolve {
  bool enabled = false;
  float threshold = 0.0f;
  float edgeRange = 0.0f;
  Vector3 edgeColor{1.0f, 1.0f, 1.0f};
  float emberAmount = 0.0f; // 縁を熾火で光らせる強さ
  // 崩壊の進み具合のうち、ディゾルブのしきい値が進んだ割合(0 〜 1)。鱗粉の量の目安(縁が進んでいる間だけ粉が出る)
  float progress = 0.0f;
};

class PlayerDeathDirector {
public:
  // 演出を始める(死亡の瞬間に 1 回呼ぶ)。すでに始まっていれば何もしない
  void Begin(const PlayerDeathSettings& settings, const PlayerDeathStart& start);

  // 演出を最初の状態へ戻す(エディタの Stop で、プレイの状態を最初へ戻すとき)
  void Reset();

  // 各サブフェーズが終わる時刻(Begin からの実時間。秒)。カメラのキーフレームの時刻に使う。Cover は、Impact の終わりと同じ
  float GetPhaseEndTime(PlayerDeathPhase phase) const;

  // 実時間の 1 フレーム分、時間を進める。
  // canSkip が true で、skipRequested が true のとき(かつ、スキップの猶予を過ぎていれば)、Cover へ直接飛ぶ。
  // 展示中は canSkip を false にして渡す
  void Update(float deltaTime, bool canSkip, bool skipRequested);

  bool IsActive() const { return phase_ != nullptr; }
  // 今の段階の名札。始まっていなければ Idle
  PlayerDeathPhase GetPhase() const { return phase_ ? phase_->GetPhase() : PlayerDeathPhase::Idle; }
  // Begin からの実時間の経過(秒)
  float GetElapsed() const { return elapsed_; }
  // 今のサブフェーズの中での進み具合(0 〜 1)。Cover は 1
  float GetPhaseProgress() const;

  // 世界の時間の速さ(1 で通常)。ヒットストップの間は 0、そのあとスローモーションへ移る。演出が始まっていなければ 1
  float GetWorldTimeScale() const;

  // 今の瞬間の、自機の位置と向き(落下の動き)
  PlayerDeathPose GetPlayerPose() const;

  // 今の瞬間の、自機の速度(ワールド。ユニット / 秒)。破片が引き継ぐ。位置を動かさない設定のときは 0
  Vector3 GetPlayerVelocity() const;

  // 崩壊の進み具合(0 〜 1)。ヒットストップの終わりから、着地(崩壊と落下の終わり)までで、0 から 1 へ進む。
  // ヒットストップの間は 0、着地のあとは 1。破片の解放が使う(破片は、落下の間じゅう、順に解放される)
  float GetCrumbleProgress() const;

  // 今の瞬間の、蝶のディゾルブ。崩壊(Crumble)の間にしきい値を上げ、崩壊が終われば、すべて消えた状態を保つ。
  // 崩壊の前(ヒットストップ中)は無効
  PlayerDeathDissolve GetDissolve() const;

  // Cover へ入ったフレームに 1 回だけ true を返す(シーンが、遷移の要求を 1 回だけ出すため)
  bool ConsumeCoverStart();

  // 蝶の崩壊を始める瞬間(ヒットストップが終わったフレーム)に 1 回だけ true を返す(自機をきりもみへ切り替えるため)
  bool ConsumeCrumbleStart();

private:
  // next へ切り替え、Enter を呼ぶ(段階が立てた合図を受け取る)
  void ChangeTo_(std::unique_ptr<IDeathPhase> next);
  // 蝶の崩壊が始まっているか(演出の最中で、ヒットストップが終わっている)
  bool HasCrumbleBegun_() const { return phase_ && phase_->HasCrumbleBegun(); }
  // 落下(ヒットストップのあとの時間 t での、落ちた距離と、落ちる速さ)
  float LandingTime_() const;
  float FallDistance_(float t) const;
  float FallSpeed_(float t) const;

  // 1 フレームに切り替える回数の上限(段階の数。段階は戻らないので、これを超えることはない。万一の無限ループを防ぐ)
  static constexpr int kMaxTransitionsPerFrame = 5;

  PlayerDeathSettings settings_;
  PlayerDeathStart start_;
  std::unique_ptr<IDeathPhase> phase_; // 今の段階(始まっていなければ nullptr)
  float elapsed_ = 0.0f;               // Begin からの実時間
  DeathPhaseSignals signals_;          // 段階が立てた合図(崩壊の始まり、Cover に入った)。Consume で 1 回だけ外へ渡す
};
