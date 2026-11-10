#pragma once
#include <string>

// ステージ選択の背景の種類(ステージの世界観ごとに 1 つ)。stages.json の "backdrop" の文字列と対応する(対応表は StageCatalog)。
// 背景を増やすときは、ここに足し、StageCatalog の対応表と、ステージ選択の背景の切り替え(StageSelectScene)に足す
enum class StageBackdrop {
  CloudSea, // 雲海と空(クリア画面などと同じ空)
};

// 1 つのステージの定義(データ)。resources/stage/stages.json の 1 項目に対応する。
// 責務は「ステージを表す情報を持つこと」だけ。読み込みは StageCatalog、ステージの受け渡しは StageLaunchParams が担当する。
// 分けた理由: ステージの追加を、コードではなくデータ(JSON)の追加で済ませるため。ステージ選択もゲームも、この構造体を読むだけにする
struct StageDefinition {
  // ステージを区別する名前(ステージ選択からゲームへ渡す荷物に入れる。重複しない)
  std::string id;
  // 画面に出す名前(日本語。ステージ選択のカードの見出しと一覧に出る)
  std::string displayName;
  // ステージの世界観が分かる短い説明文(ステージ選択のカードに出る。改行コード(バックスラッシュ n)で改行できる)
  std::string description;
  // ステージ選択で、このステージを選んだときの背景
  StageBackdrop backdrop = StageBackdrop::CloudSea;
  // シーンJSON とタイムラインJSON のファイル名の元になる名前(拡張子なし)。
  // 例: "Game" -> editor/scenes/Game.json と editor/scenes/Game_timeline.json
  std::string dataName;
  // ステージ名の板に貼る画像のパス(exe 基準。例: "resources/app/stage/stage_name_01.png")。
  // 空のときは「画像なし」で、ステージ名の板を出さずにゲームを進める(画像がまだ無いステージのため)
  std::string stageNameTexture;
  // 最初から遊べるか(false のステージは、ステージ選択でロック表示にする)
  bool unlocked = false;
};
