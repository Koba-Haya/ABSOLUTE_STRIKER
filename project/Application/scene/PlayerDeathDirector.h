#pragma once
#include "Vector.h"

// 自機の死亡演出(HP が 0 になってから、ゲームオーバー画面への遷移を要求するまで)の、時間の進行と自機の位置・向きを決めるクラス。
// StageStartDirector と同じ作り: カメラ・自機・世界は直接動かさない。シーンが Update で時間を進め、
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

// 演出の長さ・動きの設定。ステージごとに変わりうる値なので、コードに埋め込まず、まとめて持つ
namespace PlayerDeathDefaults {
// 各サブフェーズの長さ(秒)。Cover は、遷移の覆い(約 2 秒)が担うので、ここでは長さを持たない。
// 全体: HitStop + Crumble + Fall + Impact + 遷移の覆い で、6 秒以内にする
constexpr float kHitStopSeconds = 0.12f;
constexpr float kCrumbleSeconds = 1.2f;
constexpr float kFallSeconds = 1.8f;
constexpr float kImpactSeconds = 0.6f;

// ヒットストップのあとの、世界の時間の速さ(1 で通常、0 で停止)と、そこへ向かう時間(秒)
constexpr float kSlowMotionScale = 0.3f;
constexpr float kSlowMotionRampSeconds = 0.25f;

// 蝶のディゾルブ(朽ちる)。マスク(butterfly_dissolve_mask.png)の値が、しきい値以下の所から消える。
// マスクは、翅の外側ほど小さく、胴が最も大きい(0.95)ので、しきい値を 0 から終わりの値まで上げると、翅の外側から内側へ、最後に胴が朽ちる
// しきい値は、崩壊の始まり(ヒットストップの終わり)から、着地を過ぎて kDissolveTailSeconds 秒後まで、線形に上げる。
// 翅(マスク 0.10〜0.80)は着地の少し手前までに朽ち、胴(マスク 0.95)は着地まで残って、雲に沈みながら消える
constexpr float kDissolveStartThreshold = 0.0f;   // 崩壊の始まり(何も消えない)
constexpr float kDissolveEndThreshold = 1.0f;     // 崩壊の終わり(胴まで、すべて消える)
constexpr float kDissolveTailSeconds = 0.6f;      // 着地のあと、しきい値が終わりの値へ届くまでの時間(秒)
constexpr float kDissolveEdgeRange = 0.08f;       // 縁の光る帯の幅(しきい値の単位)
constexpr float kDissolveEdgeColorR = 1.00f;      // 縁の色(淡い金色)
constexpr float kDissolveEdgeColorG = 0.82f;
constexpr float kDissolveEdgeColorB = 0.45f;

// デバッグ用: true の間、死亡の位置と向きを動かさない(蝶は、死んだ場所に留まって朽ちる)。
// 崩壊(ディゾルブと破片)だけを、その場で見たいときに使う(通常は false。落下する)
constexpr bool kDebugHoldPlayerPosition = false;

// スキップを受け付け始めるまでの猶予(秒)。0 で、すぐ受け付ける
constexpr float kSkipGuardSeconds = 0.5f;

// 自機の落下の動き。前へ進む勢い(レールの速さに掛ける割合。時間とともに減る)、着地する高さ、機首が下がる量。
// 落下は、「ヒットストップのあと、崩壊と落下の終わりに、雲の上面(kLandingHeight)へちょうど着く」ように、着く時刻から逆算する
// (落ち始めはゆっくりで、だんだん速くなる放物線)。着いたあとは、その速さのまま、雲の中へ沈みながら減速する
constexpr float kForwardSpeedScale = 0.8f;      // 死亡の瞬間の、前へ進む勢い(レールの速さの割合)
constexpr float kForwardDecayPerSecond = 1.2f;  // 前へ進む勢いが減る速さ(1 秒あたりの指数的な減衰率)
constexpr float kLandingHeight = -24.0f;        // 着地する高さ(ワールド y)。雲の上面。シーンが、雲の設定の値で上書きする
constexpr float kSinkDecayPerSecond = 2.0f;     // 雲へ沈んだあと、沈む速さが減る速さ(1 秒あたりの減衰率)
constexpr float kNoseDropRadians = 0.9f;        // 機首が下がる最大の角度(ラジアン)
constexpr float kNoseDropSeconds = 1.5f;        // 機首が下がりきるまでの時間(秒)
} // namespace PlayerDeathDefaults

struct PlayerDeathSettings {
  float hitStopSeconds = PlayerDeathDefaults::kHitStopSeconds;
  float crumbleSeconds = PlayerDeathDefaults::kCrumbleSeconds;
  float fallSeconds = PlayerDeathDefaults::kFallSeconds;
  float impactSeconds = PlayerDeathDefaults::kImpactSeconds;
  float dissolveStartThreshold = PlayerDeathDefaults::kDissolveStartThreshold;
  float dissolveEndThreshold = PlayerDeathDefaults::kDissolveEndThreshold;
  float dissolveTailSeconds = PlayerDeathDefaults::kDissolveTailSeconds;
  float dissolveEdgeRange = PlayerDeathDefaults::kDissolveEdgeRange;
  Vector3 dissolveEdgeColor{PlayerDeathDefaults::kDissolveEdgeColorR, PlayerDeathDefaults::kDissolveEdgeColorG,
                            PlayerDeathDefaults::kDissolveEdgeColorB};
  bool debugHoldPlayerPosition = PlayerDeathDefaults::kDebugHoldPlayerPosition;
  float slowMotionScale = PlayerDeathDefaults::kSlowMotionScale;
  float slowMotionRampSeconds = PlayerDeathDefaults::kSlowMotionRampSeconds;
  float skipGuardSeconds = PlayerDeathDefaults::kSkipGuardSeconds;
  float forwardSpeedScale = PlayerDeathDefaults::kForwardSpeedScale;
  float forwardDecayPerSecond = PlayerDeathDefaults::kForwardDecayPerSecond;
  float landingHeight = PlayerDeathDefaults::kLandingHeight;
  float sinkDecayPerSecond = PlayerDeathDefaults::kSinkDecayPerSecond;
  float noseDropRadians = PlayerDeathDefaults::kNoseDropRadians;
  float noseDropSeconds = PlayerDeathDefaults::kNoseDropSeconds;
};

enum class PlayerDeathPhase {
  Idle,     // 始まっていない
  HitStop,
  Crumble,
  Fall,
  Impact,
  Cover
};

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
};

class PlayerDeathDirector {
public:
  // 演出を始める(死亡の瞬間に 1 回呼ぶ)。すでに始まっていれば何もしない
  void Begin(const PlayerDeathSettings& settings, const PlayerDeathStart& start);

  // 演出を最初の状態へ戻す(デバッグの蘇生)
  void Reset();

  // 各サブフェーズが終わる時刻(Begin からの実時間。秒)。カメラのキーフレームの時刻に使う。Cover は、Impact の終わりと同じ
  float GetPhaseEndTime(PlayerDeathPhase phase) const;

  // 実時間の 1 フレーム分、時間を進める。
  // canSkip が true で、skipRequested が true のとき(かつ、スキップの猶予を過ぎていれば)、Cover へ直接飛ぶ。
  // 展示中は canSkip を false にして渡す
  void Update(float deltaTime, bool canSkip, bool skipRequested);

  bool IsActive() const { return phase_ != PlayerDeathPhase::Idle; }
  PlayerDeathPhase GetPhase() const { return phase_; }
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
  void EnterPhase_(PlayerDeathPhase phase);
  // 落下(ヒットストップのあとの時間 t での、落ちた距離と、落ちる速さ)
  float LandingTime_() const;
  float FallDistance_(float t) const;
  float FallSpeed_(float t) const;
  float PhaseLength_(PlayerDeathPhase phase) const;

  PlayerDeathSettings settings_;
  PlayerDeathStart start_;
  PlayerDeathPhase phase_ = PlayerDeathPhase::Idle;
  float elapsed_ = 0.0f;      // Begin からの実時間
  float phaseElapsed_ = 0.0f; // 今のサブフェーズに入ってからの実時間
  bool coverStartPending_ = false;
  bool crumbleStartPending_ = false;
};
