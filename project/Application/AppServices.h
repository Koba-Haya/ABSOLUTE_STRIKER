#pragma once
#include "GameData.h"
#include "exhibition/ExhibitionHint.h"
#include "exhibition/ExhibitionSession.h"
#include "scene/GameAssetPreloader.h"

// アプリの寿命と同じだけ生きる部品の置き場。
//
// 分けた理由: シーンは、遷移のたびに作り直されるので、シーンをまたいで残したい状態
// (先読みした資産の保持など)は、シーンの中には置けない。かといって、グローバル変数にすると
// 持ち主が決まらず、破棄の順番も制御できない。そこで、GameApp が 1 つだけ持つこのクラスを持ち主にした。
// 何でも入れる箱にならないよう、シーンへは、このクラスごとではなく、必要な部品の参照だけを SceneFactory が渡す。
class AppServices {
public:
  // 先読みした資産の保持(タイトルが Preload、アプリ終了時に GameApp が Release を呼ぶ)
  GameAssetPreloader &GetAssetPreloader() { return assetPreloader_; }

  // 展示モードの状態と、展示用の結果づくり(ExhibitionController が書き換え、シーンが読む)
  ExhibitionSession &GetExhibition() { return exhibition_; }

  // ゲームのデータ(定義の表と、進行・継承の保存データ)。アプリの初期化で Load する
  GameData &GetGameData() { return gameData_; }

  // 展示モードの切り替え操作(TAB キー)を知らせる、画面の右上の案内(各シーンが、描画の最中に Draw を呼ぶ)
  ExhibitionHint &GetExhibitionHint() { return exhibitionHint_; }

private:
  GameAssetPreloader assetPreloader_;
  ExhibitionSession exhibition_;
  ExhibitionHint exhibitionHint_;
  GameData gameData_;
};
