#pragma once
#include "RunResult.h"

// ステージごとのベスト記録と、今回の結果による更新の内容。
// 分けた理由: 保存(StageProgress)と、結果画面の表示(ResultTally)の両方が使う型なので、どちらにも属さない小さなヘッダに置く
// (表示が、保存のクラスのヘッダまで取り込まないようにして、依存を一方向に保つ)。

// ベスト記録(スコア、評価の点数、ランクを、それぞれ別に、最高の値で持つ)
struct BestRecord {
  bool exists = false;       // 記録があるか(そのステージを、まだクリアしていなければ false)
  int score = 0;             // ベストスコア
  float points = 0.0f;       // ベストの評価の点数(合計の点数)
  RunRank rank = RunRank::C; // 最高ランク
};

// 今回の結果を記録したときの、更新の内容(結果画面が「ベスト更新」を出すために読む)
struct BestUpdate {
  BestRecord previous;          // 更新する前のベスト記録(exists が false なら、初めてのクリア)
  bool scoreImproved = false;   // ベストスコアを更新したか(初めてのクリアも true)
  bool pointsImproved = false;  // ベストの点数を更新したか(初めてのクリアも true)
  bool rankImproved = false;    // 最高ランクを上げたか(初めてのクリアは false。前のランクとの比較ができないため)
};
