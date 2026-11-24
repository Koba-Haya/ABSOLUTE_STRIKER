#pragma once
#include <string>
#include <vector>

#include "StageDefinition.h"

// ステージ定義の一覧。resources/app/stage/stages.json を読んで保持し、ID での検索を提供する。
// 責務は「読み込み、検証、検索」だけ(どのステージを遊ぶかの選択は持たない)。
// 読み込みは Load を呼んだときに行う。ファイルが無い、書式が違う、定義が 1 つも無いなどのときは、
// 探した場所を表示してアプリを終了する(空のステージ一覧で黙って進まないため。シーンJSON と同じ扱い)。
class StageCatalog {
public:
  // stages.json を読み、検証する(持ち主の GameData が、アプリの初期化で呼ぶ)。以前は Singleton で、最初の GetInstance のときに読んでいた。
  // ファイルが無い、書式が違う、定義が 1 つも無いなどのときは、探した場所を表示してアプリを終了する
  void Load();

  // すべてのステージ定義(JSON に書かれた順。ステージ選択の並びになる)
  const std::vector<StageDefinition> &GetStages() const { return stages_; }

  // ID でステージ定義を探す。無ければ nullptr
  const StageDefinition *Find(const std::string &stageId) const;

  // 既定のステージ(先頭の定義)。荷物なしでゲームに入るとき(開発のショートカットなど)に使う。
  // 読み込みに成功した時点で、定義が 1 つ以上あることは保証されている
  const StageDefinition &GetDefault() const { return stages_.front(); }

private:

  std::vector<StageDefinition> stages_;
};
