#pragma once
#include "Method.h"
#include <array>

// 気流の筋(スピード線)の本数
constexpr int kTitleStreakCount = 16;

// ある時刻のタイトル演出の状態(TitleScene がモデルとカメラへ流し込む)
struct TitleState {
  Matrix4x4 playerWorld;
  Vector3 cameraEye;
  Vector3 cameraTarget;
  Matrix4x4 logoWorld; // カメラに取り付けた板ポリ(どのカットでも同じ画面位置)
  Matrix4x4 startWorld;
  Matrix4x4 flashWorld; // カメラの目の前を覆う板(スタート時の白フラッシュ用)
  float fovY;
  // 描画用のワールド座標を巻き戻した分(z)。座標が際限なく大きくなると float の精度が荒れてカメラや自機がガクガクするので、
  // 一定距離ごとに巻き戻して小さく保っている。ワールドの位置に依存する模様(水面の波・雲)には、この分を足して渡す
  Vector3 worldOffset;
  // 気流の筋。ワールドに固定された細長い板で、自機が進むと後ろへ流れ去る(各筋のワールド行列とアルファ)
  std::array<Matrix4x4, kTitleStreakCount> streakWorld;
  std::array<float, kTitleStreakCount> streakAlpha;
};

// タイトルのスクリプト演出。時間だけから全ての状態を計算する(状態を持たない)
// - 自機: 前方(+Z)へ一定速度で飛びつつ、左右に S 字。バンクは左右速度に比例。③のカットでバレルロール
// - カメラ: 6 秒ごとに 4 カット(①ローアングル ②後方追従 ③横トラッキング ④回り込み)でループ
// - スタート演出(launchProgress が 0〜1): 自機が加速して飛び去り、カメラは遅れて付いて引き、画角が広がる
class TitleDirector {
public:
  // 自機モデルの設定。モデルを差し替えても、大きさと回転の中心が合うようにする(モデルの頂点から求めた値を渡す)
  // - center: モデル空間での中心(ここを原点に、回転・バンクする)
  // - scale: モデル空間 → ワールドの拡大率
  void SetPlayerModelFit(const Vector3 &center, float scale) {
    playerCenter_ = center;
    playerScale_ = scale;
  }

  // launchProgress: スタート入力後の進行(0〜1)。負なら通常のループ演出
  // time は double(長時間表示しても、時間の刻みの精度が落ちないように)
  TitleState Evaluate(double time, float launchProgress = -1.0f) const;

private:
  Vector3 playerCenter_{0.0f, 0.0f, 0.0f};
  float playerScale_ = 1.0f;
};
