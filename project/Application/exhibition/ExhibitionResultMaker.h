#pragma once
#include <random>

#include "../result/RatingSettings.h"
#include "../result/RunResult.h"

/// <summary>
/// 展示モード用の、固定の結果(評価と点数)を作るクラス。
/// 分けた理由: 展示では、ボス戦だけを通すので、実際の計測の点数は低い。クリア演出(結果の集計と、継承の選択)を見せるため、
/// 「こう遊んだらこのランクになる」という成績の数字(RunStats など)を、ランクごとに用意して、本物の評価(RunEvaluator)に通す。
/// こうすると、点数の内訳、ランクの境目(rating.json)と、結果画面の見せ方が、本物と必ず一致する。
/// ランクは、周回ごとにランダムに選ぶ(直前と同じランクは避ける)。出やすさは、ランクごとの重み(定数)で決める。
/// </summary>
class ExhibitionResultMaker {
public:
  /// <summary>
  /// 展示用の結果を作る。
  /// </summary>
  /// <param name="scoreTarget">ステージの目標スコア(評価のスコアの割合に使う)</param>
  /// <param name="playerMaxHp">自機の最大 HP(被ダメージの割合に使う)</param>
  /// <param name="settings">評価の設定(rating.json)</param>
  RunResult Make(int scoreTarget, int playerMaxHp, const RatingSettings &settings);

private:
  int lastPreset_ = -1;                              // 直前に出した見本の位置(同じ見本が続かないようにする。-1 は、まだ出していない)
  std::mt19937 random_{std::random_device{}()};      // 見本を選ぶ乱数
};
