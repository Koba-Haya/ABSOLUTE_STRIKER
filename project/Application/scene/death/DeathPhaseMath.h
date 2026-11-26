#pragma once
#include "PlayerDeathTypes.h"

// 自機の死亡演出の、状態を持たない計算。段階の状態クラスと、PlayerDeathDirector(落下の動き、ディゾルブ)が使う。
// 継承で共有せず、関数で共有する(ClearPhaseMath と同じ考え方)
namespace DeathPhaseMath {
// 世界の時間の速さ: 止まる(ヒットストップ)
constexpr float kStoppedTimeScale = 0.0f;

// 0 〜 1 の値を、なめらかに(両端でゆっくり)変える。範囲の外は 0 〜 1 に収める
float SmoothStep(float t);

// ヒットストップのあとの、スローモーションの世界の時間の速さ。ヒットストップが終わってから、settings.slowMotionScale へなめらかに移る。
// totalElapsed: 演出の始まりからの実時間(秒)
float SlowMotionScale(const PlayerDeathSettings &settings, float totalElapsed);
} // namespace DeathPhaseMath
