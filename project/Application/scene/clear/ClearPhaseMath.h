#pragma once
#include "ClearPhaseTypes.h"

// ボス撃破の演出の、状態を持たない計算(同じ入力なら、いつ呼んでも同じ結果)。段階の状態クラスが使う。
// 継承で共有せず、関数で共有する理由: 時間の計算は、段階の終わり方(時間 / 条件)に関係なく使えるので、基底クラスに縛らない
namespace ClearPhaseMath {
// 世界の時間の速さ: 止まる(ヒットストップ)
constexpr float kStoppedTimeScale = 0.0f;
// 世界の時間の速さ: 通常
constexpr float kNormalTimeScale = 1.0f;

// 0 〜 1 の値を、なめらかに(両端でゆっくり)変える。範囲の外は 0 〜 1 に収める
float SmoothStep(float t);

// 経過 elapsed が、長さ length のうちどこまで進んだか。長さが 0 以下なら、すでに終わった扱い(1)。範囲の外は収めない
float Ratio(float elapsed, float length);

// ヒットストップのあとの、スローモーションの世界の時間の速さ。ヒットストップが終わってから、settings.slowMotionScale へなめらかに移る。
// totalElapsed: 演出の始まりからの実時間(秒)
float SlowMotionScale(const ClearSettings &settings, float totalElapsed);
} // namespace ClearPhaseMath
