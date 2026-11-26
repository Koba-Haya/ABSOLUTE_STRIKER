#pragma once
#include "Vector.h"

// ステージ開始演出(画面遷移の覆いが晴れたあと、プレイが始まるまでの演出)のカメラの設定と、カメラのずれの計算。
// 設定はステージごとに変わる値なので、コードに埋め込まずデータとして持つ(ステージが増えたら、ステージの設定から渡す)。
//
// 以前は StageStartDirector クラスが、時間の進行と、段階(enum)の判定と、ここにある計算を、まとめて持っていた。
// 時間の進行と段階は、開幕の演出の状態クラス(OpeningSequence と各状態)へ移したので、ここには、状態を持たない計算だけを残している。
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

// 演出の今の瞬間の、カメラの基準からのずれ。すべて 0(画角は 1)のとき、通常の視点と同じ
struct StageStartPose {
  float backOffset = 0.0f;      // 進行方向の後ろへ引く距離(ユニット)
  float riseOffset = 0.0f;      // 上へ上げる高さ(ユニット)
  float lookAheadOffset = 0.0f; // 注視点を進行方向の前へ送る距離(ユニット)
  float fovScale = 1.0f;        // 画角の倍率
};

// ステージ開始演出の、状態を持たない計算(同じ入力なら、いつ呼んでも同じ結果)。
// 状態クラス(PullBackState / HoldState / ReturnState)と、GameScene(板の通り道を前もって求める)の両方が使う
namespace StageStartMath {
// elapsed 秒のときの、duration 秒かけて進む区間の、0 〜 1 の進み具合。duration が 0 以下なら、すぐ 1 にする
float Progress(float elapsed, float duration);

// 0 〜 1 を、始まりと終わりがなめらかな S 字(smoothstep)に変える。
// 傾きが始まりと終わりで 0 になるので、カメラが急に動き出したり、急に止まったりしない
float SmoothStep(float t);

// 俯瞰の度合い(blend: 0 = 通常の視点、1 = 俯瞰)から、カメラのずれを求める
StageStartPose MakePose(const StageStartSettings &settings, float blend);

// カメラの基準の位置・注視点に、ずれを重ねた位置・注視点を求める(カメラは動かさない)。
// 位置は「後ろへ引く + ワールドの上(y)へ上げる」、注視点は「前へ送る」。
// 位置が上がるだけで注視点は上げないので、見下ろす向きになる
// eye / target: 基準の位置・注視点。outEye / outTarget: 結果
void ApplyPose(const StageStartPose &pose, const Vector3 &eye, const Vector3 &target, Vector3 &outEye, Vector3 &outTarget);

// 「戻る」段階の始まりから returnElapsedSeconds 経過したときの、ブースト量(0 〜 1)。突き破りへ向けた加速の度合い。
// 板を割る時刻へ向けて、だんだん速く上がり(2 次曲線)、割れたあとは、「戻る」段階の終わりへ向けて、なめらかに 0 へ戻る。
// 時間だけで決まるので、演出を始める前に、突き破るまでに進む距離を求めるのにも使える
float BoostAt(const StageStartSettings &settings, float returnElapsedSeconds);
} // namespace StageStartMath
