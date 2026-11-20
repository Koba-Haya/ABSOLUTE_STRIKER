#pragma once
#include "RatingSettings.h"

// 評価の基準(resources/app/stage/rating.json)を読み込んで持つクラス。
// 責務は「読み込みと検証」だけ(評価の計算は RunEvaluator)。StageCatalog と同じ方針で、
// ファイルが無い、書式が違うときは、探した場所を表示してアプリを終了する(黙って既定値で進むと、基準が効いていないことに気づけないため)。
// 読み込みは、最初に GetInstance を呼んだときに 1 度だけ行う。
class RatingCatalog {
public:
  static RatingCatalog &GetInstance();

  const RatingSettings &GetSettings() const { return settings_; }

private:
  RatingCatalog();
  void Load_();

  RatingSettings settings_;
};
