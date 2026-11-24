#pragma once
#include "RatingSettings.h"

// 評価の基準(resources/app/stage/rating.json)を読み込んで持つクラス。
// 責務は「読み込みと検証」だけ(評価の計算は RunEvaluator)。StageCatalog と同じ方針で、
// ファイルが無い、書式が違うときは、探した場所を表示してアプリを終了する(黙って既定値で進むと、基準が効いていないことに気づけないため)。
// 読み込みは、Load を呼んだときに行う。
class RatingCatalog {
public:
  // rating.json を読み、検証する(持ち主の GameData が、アプリの初期化で呼ぶ)。以前は Singleton で、最初の GetInstance のときに読んでいた
  void Load();

  const RatingSettings &GetSettings() const { return settings_; }

private:
  RatingSettings settings_;
};
