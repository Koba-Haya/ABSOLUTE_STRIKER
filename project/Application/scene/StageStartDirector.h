#pragma once
#include "Vector.h"

// ステージ開始演出(画面遷移の覆いが晴れたあと、プレイが始まるまでの演出)のカメラの設定。
// ステージごとに変わる値なので、コードに埋め込まずデータとして持つ(ステージが増えたら、ステージの設定から渡す)。
namespace StageStartDefaults {
// 俯瞰で、通常のカメラ位置から進行方向の後ろへ引く距離(ユニット)
constexpr float kPullBackDistance = 12.0f;
// 俯瞰で、通常のカメラ位置から上へ上げる高さ(ユニット)
constexpr float kRiseHeight = 5.0f;
// 俯瞰で、注視点を進行方向の前へ送る距離(ユニット)。大きいほど、自機の先(最初の敵や進む先)が見える
constexpr float kLookAheadDistance = 25.0f;
// 俯瞰のときの画角の倍率(1 で通常の画角)
constexpr float kFovScale = 1.1f;
// 通常の視点から俯瞰へ引く時間(秒)
constexpr float kPullBackSeconds = 1.6f;
// 俯瞰を保つ時間(秒)。ステージ名の表示などは、この間に行う
constexpr float kHoldSeconds = 1.5f;
// 俯瞰から通常の視点へ戻る時間(秒)。この間は世界が動き、自機がステージ名の板へ向かって飛ぶ
constexpr float kReturnSeconds = 1.6f;
// 戻り始めてから、板を割る(自機が板に触れる)までの時間(秒)。kReturnSeconds より短くする
constexpr float kBreakSeconds = 1.0f;
// ブースト(突き破りへ向けた加速)が最大のときの、世界の速さの倍率(1 で加速なし)。
// 世界の更新を、1 フレームに、この回数まで回して速くする
constexpr float kBoostSpeedScale = 3.0f;
// ブーストが最大のときの、画角の追加の倍率(俯瞰の画角の倍率に、さらに掛ける。1 で広げない)
constexpr float kBoostFovScale = 1.2f;
} // namespace StageStartDefaults

struct StageStartSettings {
  float pullBackDistance = StageStartDefaults::kPullBackDistance;
  float riseHeight = StageStartDefaults::kRiseHeight;
  float lookAheadDistance = StageStartDefaults::kLookAheadDistance;
  float fovScale = StageStartDefaults::kFovScale;
  float pullBackSeconds = StageStartDefaults::kPullBackSeconds;
  float holdSeconds = StageStartDefaults::kHoldSeconds;
  float returnSeconds = StageStartDefaults::kReturnSeconds;
  float breakSeconds = StageStartDefaults::kBreakSeconds;
  float boostSpeedScale = StageStartDefaults::kBoostSpeedScale;
  float boostFovScale = StageStartDefaults::kBoostFovScale;
};

// ステージ開始演出の段階
enum class StageStartPhase {
  Idle,     // 始まっていない、または終わった
  PullBack, // 通常の視点から俯瞰へ引く(世界は止まっている)
  Hold,     // 俯瞰を保つ(世界は止まっている)
  Return    // 俯瞰から通常の視点へ戻る(世界が動き、自機が板へ向かって飛ぶ)
};

// 演出の今の瞬間の、カメラの基準からのずれ。すべて 0(画角は 1)のとき、通常の視点と同じ
struct StageStartPose {
  float backOffset = 0.0f;      // 進行方向の後ろへ引く距離(ユニット)
  float riseOffset = 0.0f;      // 上へ上げる高さ(ユニット)
  float lookAheadOffset = 0.0f; // 注視点を進行方向の前へ送る距離(ユニット)
  float fovScale = 1.0f;        // 画角の倍率
};

// ステージ開始演出の時間の進行と、各瞬間のカメラのずれを求める。
// カメラや自機は直接動かさない(動かすのはシーン側)。シーンは Update で時間を進め、GetPose の値をカメラへ反映する。
// 俯瞰へ引く、保つ、戻るの 3 段階を、経過時間から求める。デモプレイで繰り返し使えるよう、Begin で必ず最初から始める。
class StageStartDirector {
public:
  // 演出を最初から始める。前回の状態は残さない
  void Begin(const StageStartSettings &settings);

  // 時間を進める。全体の長さを超えたら、演出を終える
  // deltaTime: 進める時間(秒)
  void Update(float deltaTime);

  // 演出が進行中なら true
  bool IsActive() const { return active_; }

  // 今の段階
  StageStartPhase GetPhase() const;

  // 世界(レール・敵・自機)を動かす段階なら true。俯瞰へ引く・保つ間は止める。
  // 動かし始めるのは「戻る」段階から。戻りながら自機が前へ飛び、板を突き破る
  bool IsWorldRunning() const { return GetPhase() == StageStartPhase::Return; }

  // 板を割る時刻に達していたら true(「戻る」段階の始まりから breakSeconds 経過後)。
  // 時間で決めるので、毎回同じタイミングで割れる
  bool HasReachedBreak() const;

  // 今の段階の中での進み具合(0 = 段階の始まり、1 = 段階の終わり)。始まっていないときは 0。
  // ステージ名の表示の出し入れなど、段階に合わせた演出のタイミングに使う
  float GetPhaseProgress() const;

  // 「戻る」段階の始まりからの経過(秒)。「戻る」段階でなければ 0
  float GetReturnElapsedSeconds() const;

  // ブースト量(0 〜 1)。突き破りへ向けた加速の度合いで、世界の速さと画角の広がりに使う。
  // 「戻る」段階の始まりから、板を割る時刻へ向けて、だんだん速く上がり(2 次曲線)、
  // 割れたあとは、「戻る」段階の終わりへ向けて、なめらかに 0 へ戻る。「戻る」段階の外では 0
  float GetBoost() const;

  // 「戻る」段階の始まりから returnElapsedSeconds 経過したときの、ブースト量。
  // 時間だけで決まる計算なので、演出を始める前に、突き破るまでに進む距離を求めるのにも使える
  float BoostAt(float returnElapsedSeconds) const;

  // 俯瞰を保っているとき(俯瞰の度合いが最大)の、カメラのずれ。
  // 「戻る」段階の前でも、俯瞰のカメラが最終的にどこにあるかを求めたいときに使う
  StageStartPose GetHoldPose() const;

  // カメラの基準の位置・注視点に、ずれを重ねた位置・注視点を求める(純粋な計算。カメラは動かさない)。
  // 位置は「後ろへ引く + ワールドの上(y)へ上げる」、注視点は「前へ送る」。
  // 位置が上がるだけで注視点は上げないので、見下ろす向きになる
  // eye / target: 基準の位置・注視点。outEye / outTarget: 結果
  static void ApplyPose(const StageStartPose &pose, const Vector3 &eye, const Vector3 &target, Vector3 &outEye, Vector3 &outTarget);

  // 今の瞬間の、カメラの基準からのずれ。演出が終わったあとは、通常の視点と同じ値を返す
  StageStartPose GetPose() const;

private:
  // 俯瞰の度合い(blend: 0 = 通常の視点、1 = 俯瞰)から、カメラのずれを求める
  StageStartPose MakePose_(float blend) const;
  // 演出全体の長さ(秒)
  float TotalSeconds_() const;
  // 俯瞰の度合い(0 = 通常の視点、1 = 俯瞰)を、今の経過時間から求める。なめらかに始まり、なめらかに止まる
  float CalcBlend_() const;

  StageStartSettings settings_;
  float elapsed_ = 0.0f; // 始まってからの経過(秒)
  bool active_ = false;
};
