#include "PostEffectDirector.h"
#include "Renderer.h"
#include <algorithm> // std::clamp
#include <cmath>

namespace {
// 加速中のブラーの、最大の強さ(RadialBlur の幅)
constexpr float kSpeedBlurIntensity     = 0.03f;
// これ以下の強さのブラーは、掛けない(わずかなブラーのために、ポストエフェクトを切り替えないようにする)
constexpr float kSpeedBlurMin           = 0.01f;
constexpr float kBossIntroDuration      = 1.2f;
// 被弾の Vignette(画面の周囲が暗くなる)の長さ(秒)。Hit のアニメーションが見えるよう、元の 0.3 の半分にした
constexpr float kPlayerHitDuration      = 0.15f;

// 演出パラメータ（トリガー時に一度だけ設定する）
// Vignette の明るさの式: 明るさ = saturate((周囲ほど小さい値 * kVignetteScale) ^ kVignettePow)。
// 被弾の Vignette は、周囲が暗くなりすぎて Hit のアニメーションが見づらかったので、乗数を 0.5 から 0.25 に下げて、
// 周囲の暗さを約半分にした(例: 画面の左端の中央で、暗さが 約 40% から 約 23%)。さらに調整するなら、この値(小さいほど弱い)と長さ
constexpr float kVignetteScale = 16.0f;
constexpr float kVignettePow   = 0.25f;
// 瀕死(HP が kCriticalHp 以下で、まだ生きている間)の、画面の周囲の赤い枠。静かに脈打たせる程度にして、
// 被弾のアニメーションの邪魔にならないようにする。強さの最小・最大(0 〜 1)と、1 回脈打つ時間(秒)は、実機で見て調整する
constexpr int   kCriticalHp = 1;
constexpr float kCriticalPulsePeriod      = 1.6f;
constexpr float kCriticalStrengthMin      = 0.30f;
constexpr float kCriticalStrengthMax      = 0.60f;
constexpr float kCriticalVignetteScale    = 16.0f; // 枠の形(被弾の暗転と同じ式)
constexpr float kCriticalVignettePow      = 1.0f;  // 大きいほど、赤が中心側まで広がる
constexpr float kTwoPi                    = 6.28318530718f;
const Vector3 kCriticalColor{ 0.85f, 0.03f, 0.03f };   // 枠の色(暗めの赤)
constexpr int   kBossIntroBlurKernel = 5;
constexpr float kBossIntroBlurSigma  = 4.0f;
} // namespace

void PostEffectDirector::OnBossIntro() {
  bossIntroBlur_.Start(kBossIntroDuration);
  Renderer::GetInstance()->SetGaussianFilterParam(kBossIntroBlurKernel, kBossIntroBlurSigma, { 1.0f, 0.0f });
}

void PostEffectDirector::Update(float deltaTime, const FrameState& state) {
  auto* renderer = Renderer::GetInstance();

  // --- タイマー更新 ---
  bossIntroBlur_.Tick(deltaTime);
  playerHitVignette_.Tick(deltaTime);
  criticalPulseTimer_ += deltaTime;

  // --- 被弾検知：前フレームよりHPが減っていたらVignetteを発火 ---
  bool isCriticalHp = false;
  if (state.hasPlayer) {
    isCriticalHp = state.playerHp > 0 && state.playerHp <= kCriticalHp;
    if (lastPlayerHp_ >= 0 && state.playerHp < lastPlayerHp_) {
      playerHitVignette_.Start(kPlayerHitDuration);
      renderer->SetVignetteParam(kVignetteScale, kVignettePow);
    }
    lastPlayerHp_ = state.playerHp;
  }

  // --- 優先度チェーン（ヘッダのコメント参照） ---
  Renderer::PostProcessMode nextMode = Renderer::PostProcessMode::Normal;
  if (state.speedBlur > kSpeedBlurMin) {
    // 加速中のブラー: ブースト量に比例した強さの RadialBlur
    renderer->SetRadialBlurParam(state.speedBlurCenterUv, kSpeedBlurIntensity * std::clamp(state.speedBlur, 0.0f, 1.0f));
    nextMode = Renderer::PostProcessMode::RadialBlur;
  } else if (playerHitVignette_.IsActive()) {
    nextMode = Renderer::PostProcessMode::Vignette;
  } else if (bossIntroBlur_.IsActive()) {
    nextMode = Renderer::PostProcessMode::GaussianFilter;
  } else if (isCriticalHp) {
    // 瀕死: 周囲の枠だけが、ゆっくり脈打つように赤くなる(強さを、最小と最大の間で、正弦波で変える)
    const float wave = 0.5f + 0.5f * std::sin(criticalPulseTimer_ / kCriticalPulsePeriod * kTwoPi);
    const float strength = kCriticalStrengthMin + (kCriticalStrengthMax - kCriticalStrengthMin) * wave;
    renderer->SetVignetteParam(kCriticalVignetteScale, kCriticalVignettePow, kCriticalColor, strength);
    nextMode = Renderer::PostProcessMode::Vignette;
  } else if (state.isBossPhase) {
    nextMode = Renderer::PostProcessMode::LuminanceBasedOutline;
  }
  renderer->SetPostProcessMode(nextMode);
}
