#pragma once
#include <string>

// 1 つのステージの定義(データ)。resources/stage/stages.json の 1 項目に対応する。
// 責務は「ステージを表す情報を持つこと」だけ。読み込みは StageCatalog、ステージの受け渡しは StageLaunchParams が担当する。
// 分けた理由: ステージの追加を、コードではなくデータ(JSON)の追加で済ませるため。ステージ選択もゲームも、この構造体を読むだけにする
struct StageDefinition {
  // ステージを区別する名前(ステージ選択からゲームへ渡す荷物に入れる。重複しない)
  std::string id;
  // 画面に出す名前
  std::string displayName;
  // シーンJSON とタイムラインJSON のファイル名の元になる名前(拡張子なし)。
  // 例: "Game" -> editor/scenes/Game.json と editor/scenes/Game_timeline.json
  std::string dataName;
  // ステージ名の板に貼る画像のパス(exe 基準。例: "resources/app/stage/stage_name_01.png")
  std::string stageNameTexture;
  // 最初から遊べるか(false のステージは、ステージ選択でロック表示にする)
  bool unlocked = false;
};
