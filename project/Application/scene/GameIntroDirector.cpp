#include "GameIntroDirector.h"

#include <algorithm>
#include <cmath>

namespace {
// 開始時の画角の倍率。広めの画角から通常へ落ち着かせると、加速して着地した感じが出る
constexpr float kStartFovScale = 1.35f;
// 雲の層の中から昇り始める高さ(スタート位置からの下げ幅)。画面遷移の表の、ゲーム側の層(kGameSlabBottom 〜 kGameSlabTop)
// の中から始まり、雲海の上面を抜けて、スタート位置へ昇る
constexpr float kStartRise = 50.0f;
// 昇り始めの機首の上げ角(ラジアン)。タイトルのスタート演出の終わりの角度と合わせる
constexpr float kStartPitchUp = 0.42f;
constexpr float kPitchFalloffPower = 1.5f;
} // namespace

GameIntroPose GameIntroDirector::Evaluate(float revealProgress, float railSpeed, float revealSeconds) {
  const float t = std::clamp(revealProgress, 0.0f, 1.0f);

  GameIntroPose pose;
  // 助走は等速(railSpeed)で、終わりにスタート位置へ着く。残りの距離 = 全体の距離 * (1 - 進み具合)
  pose.backOffset = railSpeed * revealSeconds * (1.0f - t);

  // 昇りは、はじめが速く、だんだん緩やかになる(2 次の ease out)。スタート位置へ着くときに、上下の動きが止まる
  const float left = 1.0f - t;
  pose.riseOffset = kStartRise * left * left;
  pose.pitchUp = kStartPitchUp * std::pow(left, kPitchFalloffPower);

  // 画角は、終わりへ向けてなめらかに通常へ戻す(ease out)
  const float remaining = 1.0f - t;
  const float eased = 1.0f - remaining * remaining * remaining;
  pose.fovScale = kStartFovScale + (1.0f - kStartFovScale) * eased;
  return pose;
}
