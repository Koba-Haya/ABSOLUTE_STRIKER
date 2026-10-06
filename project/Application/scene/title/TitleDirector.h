#pragma once
#include "Method.h"

// ある時刻のタイトル演出の状態(TitleScene がモデルとカメラへ流し込む)
struct TitleState {
  Matrix4x4 playerWorld;
  Vector3 cameraEye;
  Vector3 cameraTarget;
  Matrix4x4 logoWorld; // カメラに取り付けた板ポリ(どのカットでも同じ画面位置)
  Matrix4x4 startWorld;
  Matrix4x4 flashWorld; // カメラの目の前を覆う板(スタート時の白フラッシュ用)
  float fovY;
};

// タイトルのスクリプト演出。時間だけから全ての状態を計算する(状態を持たない)
// - 自機: 前方(+Z)へ一定速度で飛びつつ、左右に S 字。バンクは左右速度に比例。③のカットでバレルロール
// - カメラ: 6 秒ごとに 4 カット(①ローアングル ②後方追従 ③横トラッキング ④回り込み)でループ
// - スタート演出(launchProgress が 0〜1): 自機が加速して飛び去り、カメラは遅れて付いて引き、画角が広がる
class TitleDirector {
public:
  // launchProgress: スタート入力後の進行(0〜1)。負なら通常のループ演出
  TitleState Evaluate(float time, float launchProgress = -1.0f) const;
};
