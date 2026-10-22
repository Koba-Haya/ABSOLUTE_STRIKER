#include "PostEffectDirector.h"
#include "Renderer.h"
#include <algorithm> // std::clamp
#include <cstdlib> // rand()

namespace {
// 各演出の継続時間（秒）・強さ
constexpr float kCameraShakeDuration    = 0.3f;
constexpr float kCameraShakeIntensity   = 0.2f;
constexpr float kHitDistortionDuration  = 0.25f;
constexpr float kHitDistortionIntensity = 0.06f;
// 加速中のブラーの、最大の強さ(RadialBlur の幅。ヒット時の歪み 0.06 より弱くして、少しだけ掛ける)
constexpr float kSpeedBlurIntensity     = 0.03f;
// これ以下の強さのブラーは、掛けない(わずかなブラーのために、ポストエフェクトを切り替えないようにする)
constexpr float kSpeedBlurMin           = 0.01f;
constexpr float kBossIntroDuration      = 1.2f;
constexpr float kPlayerHitDuration      = 0.3f;
constexpr float kWaveSpawnDuration      = 0.5f;
constexpr float kKillStreakDuration     = 0.4f;

// 演出パラメータ（トリガー時に一度だけ設定する）
constexpr float kVignetteScale = 16.0f;
constexpr float kVignettePow   = 0.5f;
constexpr int   kBossIntroBlurKernel = 5;
constexpr float kBossIntroBlurSigma  = 4.0f;
constexpr int   kWaveSpawnBoxKernel  = 4;

constexpr int kCriticalHp = 1; // このHP以下で瀕死演出（Grayscale）
} // namespace

void PostEffectDirector::OnEnemyDestroyed(const Vector2& radialBlurCenterUv) {
  cameraShake_.Start(kCameraShakeDuration);
  cameraShakeIntensity_ = kCameraShakeIntensity;

  hitDistortion_.Start(kHitDistortionDuration);
  hitDistortionIntensity_ = kHitDistortionIntensity;
  radialBlurCenter_ = radialBlurCenterUv;
}

void PostEffectDirector::OnKillStreak() {
  killStreakGlitch_.Start(kKillStreakDuration);
}

void PostEffectDirector::OnBossIntro() {
  bossIntroBlur_.Start(kBossIntroDuration);
  Renderer::GetInstance()->SetGaussianFilterParam(kBossIntroBlurKernel, kBossIntroBlurSigma, { 1.0f, 0.0f });
}

void PostEffectDirector::OnWaveSpawn() {
  waveSpawnBlur_.Start(kWaveSpawnDuration);
  Renderer::GetInstance()->SetBoxFilterParam(kWaveSpawnBoxKernel);
}

void PostEffectDirector::Update(float deltaTime, float totalTime, const FrameState& state) {
  auto* renderer = Renderer::GetInstance();

  // --- タイマー更新 ---
  // カメラシェイク
  if (cameraShake_.IsActive()) {
    cameraShake_.timer += deltaTime;
    const float t = 1.0f - (cameraShake_.timer / cameraShake_.duration);
    const float intensity = cameraShakeIntensity_ * t;
    const float rX = ((rand() % 100) / 100.0f * 2.0f - 1.0f) * intensity;
    const float rY = ((rand() % 100) / 100.0f * 2.0f - 1.0f) * intensity;
    cameraShakeOffset_ = { rX, rY, 0.0f };
  } else {
    cameraShakeOffset_ = { 0.0f, 0.0f, 0.0f };
  }

  // 画面歪み（RadialBlur）：強さを時間で減衰させ、終了時に0へ戻す
  if (hitDistortion_.IsActive()) {
    hitDistortion_.timer += deltaTime;
    const float t = 1.0f - (hitDistortion_.timer / hitDistortion_.duration);
    renderer->SetRadialBlurParam(radialBlurCenter_, hitDistortionIntensity_ * t);
  } else if (hitDistortion_.duration > 0.0f) {
    renderer->SetRadialBlurParam(radialBlurCenter_, 0.0f);
    hitDistortion_.duration = 0.0f;
  }

  bossIntroBlur_.Tick(deltaTime);
  playerHitVignette_.Tick(deltaTime);
  waveSpawnBlur_.Tick(deltaTime);
  killStreakGlitch_.Tick(deltaTime);

  // --- 被弾検知：前フレームよりHPが減っていたらVignetteを発火 ---
  bool isCriticalHp = false;
  if (state.hasPlayer) {
    isCriticalHp = state.playerHp <= kCriticalHp;
    if (lastPlayerHp_ >= 0 && state.playerHp < lastPlayerHp_) {
      playerHitVignette_.Start(kPlayerHitDuration);
      renderer->SetVignetteParam(kVignetteScale, kVignettePow);
    }
    lastPlayerHp_ = state.playerHp;
  }

  // --- 優先度チェーン（ヘッダのコメント参照） ---
  Renderer::PostProcessMode nextMode = Renderer::PostProcessMode::Normal;
  if (hitDistortion_.IsActive()) {
    nextMode = Renderer::PostProcessMode::RadialBlur;
  } else if (state.speedBlur > kSpeedBlurMin) {
    // 加速中のブラー: ブースト量に比例した強さの RadialBlur(ヒット時の歪みと同じモード。ヒット中は、そちらを優先する)
    renderer->SetRadialBlurParam(state.speedBlurCenterUv, kSpeedBlurIntensity * std::clamp(state.speedBlur, 0.0f, 1.0f));
    nextMode = Renderer::PostProcessMode::RadialBlur;
  } else if (playerHitVignette_.IsActive()) {
    nextMode = Renderer::PostProcessMode::Vignette;
  } else if (bossIntroBlur_.IsActive()) {
    nextMode = Renderer::PostProcessMode::GaussianFilter;
  } else if (killStreakGlitch_.IsActive()) {
    renderer->SetRandomParam(totalTime);
    nextMode = Renderer::PostProcessMode::Random;
  } else if (waveSpawnBlur_.IsActive()) {
    nextMode = Renderer::PostProcessMode::BoxFilter;
  } else if (state.hasPlayer && state.isLockingMode) {
    nextMode = Renderer::PostProcessMode::DepthBasedOutline;
  } else if (isCriticalHp) {
    nextMode = Renderer::PostProcessMode::Grayscale;
  } else if (state.isBossPhase) {
    nextMode = Renderer::PostProcessMode::LuminanceBasedOutline;
  }
  renderer->SetPostProcessMode(nextMode);
}
