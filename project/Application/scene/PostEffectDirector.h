#pragma once
#include "Type/Vector.h"

// ゲーム中のポストエフェクトとカメラシェイクの発火・タイマー管理。
// フルスクリーンの PostProcessMode は同時に1つしか有効にできないため、
// 複数のゲームイベントを優先度チェーンで調停し、SetPostProcessMode を1フレームに1回だけ呼ぶ。
// GameScene は「イベントの通知」と「毎フレームの Update」だけを行う。
//
// 優先度（高い順）:
//   1. RadialBlur            : 雑魚敵ヒット時の画面歪み
//   2. RadialBlur            : 加速中の画面のブラー(ステージ開始演出の突き破りへ向けた加速。強さは FrameState::speedBlur)
//   3. Vignette              : プレイヤー被弾時の画面フラッシュ
//   4. GaussianFilter        : ボス出現演出
//   5. Random                : キルストリーク演出
//   6. BoxFilter             : 敵出現（新ウェーブ）時のフォーカスぼかし
//   7. DepthBasedOutline     : ロックオン中
//   8. Grayscale             : プレイヤー瀕死（HP<=1）
//   9. LuminanceBasedOutline : ボス戦中
//  10. Normal
class PostEffectDirector {
public:
  // 毎フレームの状態（チェーンの常時系エフェクトと被弾検知に使う）
  struct FrameState {
    bool hasPlayer = false;       // プレイヤーが存在するか（false の間は isLockingMode / playerHp を無視する）
    bool isLockingMode = false;   // ロックオン長押し中か
    int  playerHp = 0;            // プレイヤーの現在HP
    bool isBossPhase = false;     // ボス戦中か
    float speedBlur = 0.0f;       // 加速中のブラーの強さ(0 〜 1。0 でブラーなし)。ステージ開始演出のブースト量を渡す
    Vector2 speedBlurCenterUv{ 0.5f, 0.5f }; // 加速のブラーの中心(スクリーン座標 UV 0〜1。自機のいる辺り)
  };

  // タイマーを進め、今フレームの PostProcessMode を確定する。
  // totalTime は Random エフェクトのシード用の経過時間（秒）。
  void Update(float deltaTime, float totalTime, const FrameState& state);

  // --- イベント通知 ---
  // 敵撃破：カメラシェイクと画面歪みを開始。歪みの中心は敵のスクリーン座標(UV 0〜1)。
  void OnEnemyDestroyed(const Vector2& radialBlurCenterUv);
  void OnKillStreak();
  void OnBossIntro();
  // 敵出現（新ウェーブ）
  void OnWaveSpawn();

  // カメラシェイクのオフセット（毎フレーム Update で更新）
  const Vector3& GetCameraShakeOffset() const { return cameraShakeOffset_; }

private:
  // 開始からの経過時間で有効/無効を判定する単純なタイマー
  struct Timed {
    float timer = 0.0f;
    float duration = 0.0f;

    bool IsActive() const { return timer < duration; }
    void Start(float newDuration) { timer = 0.0f; duration = newDuration; }
    // 有効な間だけ進める。終わったら duration を 0 にして「終了済み」を区別できるようにする
    void Tick(float deltaTime) {
      if (IsActive()) timer += deltaTime;
      else duration = 0.0f;
    }
  };

  // カメラシェイク
  Timed cameraShake_;
  float cameraShakeIntensity_ = 0.0f;
  Vector3 cameraShakeOffset_{ 0.0f, 0.0f, 0.0f };

  // 画面歪み（RadialBlur）
  Timed hitDistortion_;
  float hitDistortionIntensity_ = 0.0f;
  Vector2 radialBlurCenter_{ 0.5f, 0.5f };

  Timed bossIntroBlur_;       // ボス出現（GaussianFilter）
  Timed playerHitVignette_;   // 被弾（Vignette）
  Timed waveSpawnBlur_;       // 敵出現（BoxFilter）
  Timed killStreakGlitch_;    // キルストリーク（Random）

  int lastPlayerHp_ = -1;     // 前フレームのHP。減少を検知したらVignetteを発火させる
};
