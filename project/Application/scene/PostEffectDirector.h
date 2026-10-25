#pragma once
#include "Type/Vector.h"

// ゲーム中のポストエフェクトの発火・タイマー管理。
// フルスクリーンの PostProcessMode は同時に1つしか有効にできないため、
// 複数のゲームイベントを優先度チェーンで調停し、SetPostProcessMode を1フレームに1回だけ呼ぶ。
// GameScene は「イベントの通知」と「毎フレームの Update」だけを行う。
//
// 優先度（高い順）:
//   1. RadialBlur            : 加速中の画面のブラー(ステージ開始演出の突き破りへ向けた加速。強さは FrameState::speedBlur)
//   2. Vignette(黒)         : プレイヤー被弾時の画面の暗転
//   3. GaussianFilter        : ボス出現演出
//   4. Vignette(赤)         : プレイヤー瀕死(HP が 1 のあいだ)。画面の周囲の枠だけが、ゆっくり脈打つように赤くなる
//                              (被弾の暗転とボス出現の演出の下、ボス戦のアウトラインの上。ボス戦でも見えるように)
//   5. LuminanceBasedOutline : ボス戦中
//   6. Normal
class PostEffectDirector {
public:
  // 毎フレームの状態（チェーンの常時系エフェクトと被弾検知に使う）
  struct FrameState {
    bool hasPlayer = false;       // プレイヤーが存在するか（false の間は playerHp を無視する）
    int  playerHp = 0;            // プレイヤーの現在HP
    bool isBossPhase = false;     // ボス戦中か
    float speedBlur = 0.0f;       // 加速中のブラーの強さ(0 〜 1。0 でブラーなし)。ステージ開始演出のブースト量を渡す
    Vector2 speedBlurCenterUv{ 0.5f, 0.5f }; // 加速のブラーの中心(スクリーン座標 UV 0〜1。自機のいる辺り)
  };

  // タイマーを進め、今フレームの PostProcessMode を確定する。
  void Update(float deltaTime, const FrameState& state);

  // --- イベント通知 ---
  void OnBossIntro();

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

  Timed bossIntroBlur_;       // ボス出現（GaussianFilter）
  Timed playerHitVignette_;   // 被弾（Vignette）

  float criticalPulseTimer_ = 0.0f; // 瀕死の赤い枠の脈打ちの経過時間(秒)
  int lastPlayerHp_ = -1;     // 前フレームのHP。減少を検知したらVignetteを発火させる
};
