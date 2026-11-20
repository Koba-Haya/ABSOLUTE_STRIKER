#pragma once
#include "RunStats.h"

// 評価のランク(低い順。数値の大小が、ランクの高低と対応する)
enum class RunRank {
  C,
  B,
  A,
  S,
};

// ランクを、画面やログに出す文字("S" など)にする
inline const char *RankToString(RunRank rank) {
  switch (rank) {
  case RunRank::S: return "S";
  case RunRank::A: return "A";
  case RunRank::B: return "B";
  case RunRank::C: return "C";
  }
  return "C"; // 到達しない(列挙の追加忘れがあっても、最低のランクで続ける)
}

// 評価の内訳 1 項目(結果画面で、1 つずつ見せる)
struct RatingEntry {
  float ratio = 0.0f;  // 達成の割合(0 〜 1)
  float points = 0.0f; // 点数(重み x 割合)
};

// 1 回のプレイの結果(計測した数字と、評価の点数、ランク)。リザルト画面が、これを読む。
// StageLaunchParams に任意で載せて、クリア画面へ渡す。値のコピーができること(std::any の条件)
struct RunResult {
  RunStats stats;  // 計測した数字
  int score = 0;   // スコア(ScoreTracker)
  int kills = 0;   // 撃破数(ScoreTracker)
  int scoreTarget = 0; // 評価に使った、ステージの目標スコア(結果画面で、「目標」として見せる)

  // 評価の内訳(RunEvaluator が決める)
  RatingEntry scoreEntry;       // スコア(目標スコアに対する割合)
  RatingEntry killEntry;        // 撃破率
  RatingEntry damageEntry;      // 被ダメージの少なさ
  RatingEntry formationEntry;   // 編隊を全部倒し切った割合
  RatingEntry multiLockEntry;   // マルチロック(同時ロック数と、一斉撃破)
  RatingEntry bossNoDamageEntry; // ボス戦のノーダメージ

  float totalPoints = 0.0f; // 合計の点数(0 〜 重みの合計)
  RunRank rank = RunRank::C;
};
