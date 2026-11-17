#pragma once

// 評価の基準(重み、目標、ランクのしきい値)。resources/app/stage/rating.json から読む(RatingCatalog)。
// 分けた理由: 評価の基準は、プレイして調整する値なので、コード(RunEvaluator)に埋め込まず、データとして持つ。
// 重みの合計が、満点になる(100 にそろえておくと、点数が、そのまま百点満点になる)。
struct RatingSettings {
  // 重み(各指標の満点の点数)
  float weightScore = 25.0f;        // スコア
  float weightKill = 20.0f;         // 撃破率
  float weightDamage = 25.0f;       // 被ダメージの少なさ
  float weightFormation = 10.0f;    // 編隊の全滅
  float weightMultiLock = 10.0f;    // マルチロック
  float weightBossNoDamage = 10.0f; // ボス戦のノーダメージ

  // マルチロックの目標(これに届くと、その項目は満点)
  int lockTarget = 6;           // 同時ロック数
  int killsInWindowTarget = 4;  // 一斉撃破の数

  // ランクのしきい値(この点数以上で、そのランク。これ未満は C)
  float thresholdS = 90.0f;
  float thresholdA = 75.0f;
  float thresholdB = 55.0f;
};
