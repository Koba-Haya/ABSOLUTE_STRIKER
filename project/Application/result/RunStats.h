#pragma once

// 1 回のプレイ(ステージ 1 本)の成績の元になる、計測した数字の一式(値のコピーを、評価やログへ渡すための、ただのデータ)。
// 分けた理由: 計測(RunStatsTracker)、評価(RunEvaluator)、結果の型(RunResult)の 3 つが、この型を共通で使う。
// 計測のクラス(RunStatsTracker)に置くと、評価や結果が、計測の実装の詳細(deque など)まで取り込んでしまうため、小さなヘッダに分けた。
struct RunStats {
  int enemiesSpawned = 0;        // 出現した敵の数(自滅した敵も含む)
  int enemiesKilled = 0;         // 自機が倒した敵の数
  int enemiesSelfDestructed = 0; // 自滅した敵の数(体当たりなど。撃破率の母数から外す)
  int formationsSpawned = 0;     // 出現した編隊の数
  int formationsCleared = 0;     // 全部倒し切った編隊の数
  int damageTaken = 0;           // 被ダメージの合計(HP の減った量)
  int hitCount = 0;              // 被弾の回数
  int bossHitCount = 0;          // ボス戦の間の被弾の回数
  float playSeconds = 0.0f;      // プレイ時間(秒。評価には入れない。結果画面で、参考として見せるかは未定)
  int maxLockCount = 0;          // 同時ロック数の最大
  int maxKillsInWindow = 0;      // 短い時間の窓の中での撃破数の最大(一斉撃破の目安)
};
