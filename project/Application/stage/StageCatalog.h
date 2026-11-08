#pragma once
#include <string>
#include <vector>

#include "StageDefinition.h"

// ステージ定義の一覧。resources/app/stage/stages.json を読んで保持し、ID での検索を提供する。
// 責務は「読み込み、検証、検索」だけ(どのステージを遊ぶかの選択は持たない)。
// 読み込みは最初に GetInstance を呼んだときに 1 度だけ行う。ファイルが無い、書式が違う、定義が 1 つも無いなどのときは、
// 探した場所を表示してアプリを終了する(空のステージ一覧で黙って進まないため。シーンJSON と同じ扱い)。
class StageCatalog {
public:
  // 唯一のインスタンス(初回の呼び出しで stages.json を読む)
  static StageCatalog &GetInstance();

  // すべてのステージ定義(JSON に書かれた順。ステージ選択の並びになる)
  const std::vector<StageDefinition> &GetStages() const { return stages_; }

  // ID でステージ定義を探す。無ければ nullptr
  const StageDefinition *Find(const std::string &stageId) const;

  // 既定のステージ(先頭の定義)。荷物なしでゲームに入るとき(開発のショートカットなど)に使う。
  // 読み込みに成功した時点で、定義が 1 つ以上あることは保証されている
  const StageDefinition &GetDefault() const { return stages_.front(); }

private:
  StageCatalog();
  // stages.json を読み、検証して stages_ へ入れる。失敗したときは終了する
  void Load_();

  std::vector<StageDefinition> stages_;
};
