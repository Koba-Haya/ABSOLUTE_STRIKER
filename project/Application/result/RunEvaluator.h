#pragma once
#include "RatingSettings.h"
#include "RunResult.h"

// 計測した成績(RunStats)から、評価の点数とランク(RunResult)を決める関数をまとめたクラス。
//
// 分けた理由: 「数える」(RunStatsTracker)と「評価する」(ここ)を別の責務にして、基準(重み、しきい値)を変えても、計測を触らないため。
// 状態を持たない純粋な計算(同じ入力なら、同じ結果)なので、ゲームを動かさなくても、値を渡して確かめられる。
//
// 評価(点数制): 各指標を 0 〜 1 の割合にして、重みを掛けて足す。
//   スコア         : スコア / ステージの目標スコア(上限 1)
//   撃破率         : 倒した数 / (出現した数 - 自滅した数)。敵がいなければ 1
//   被ダメージ     : 1 - 被ダメージ / 最大 HP(下限 0)。ノーダメージが 1
//   編隊の全滅     : 全滅させた編隊 / 出現した編隊。編隊が無ければ 1
//   マルチロック   : (同時ロック数の最大 / 目標 + 一斉撃破の最大 / 目標) / 2(それぞれ上限 1)
//   ボス戦ノーダメージ: ボス戦の被弾が 0 なら 1、そうでなければ 0
//   クリアタイムは、評価に入れない(固定レールで、自分で急いで進めないため)
// ランクは、合計の点数が、S、A、B のしきい値以上かで決まる(未満は C)。
class RunEvaluator {
public:
  // stats: 計測した数字。score / kills: ScoreTracker の値。
  // scoreTarget: そのステージの目標スコア(StageDefinition)。0 以下なら、スコアの項目は満点として扱う(基準が無いので、減点しない)
  // playerMaxHp: 自機の最大 HP(被ダメージの割合の基準)
  static RunResult Evaluate(const RunStats &stats, int score, int kills, int scoreTarget, int playerMaxHp, const RatingSettings &settings);

  // 点数から、ランクを決める
  static RunRank DecideRank(float totalPoints, const RatingSettings &settings);
};
