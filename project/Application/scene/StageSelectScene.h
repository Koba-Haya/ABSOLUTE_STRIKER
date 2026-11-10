#pragma once
#include "BaseScene.h"
#include "ResultBackdrop.h"
#include "BitmapFont.h"
#include "ui/StageCard.h"
#include "ui/UiMenuList.h"
#include "ui/UiPanel.h"

struct StageDefinition;

// ステージ選択のシーン(レイアウト B)。画面の中央に、選んだステージの「カード」(StageCard)、その下にステージの一覧(UiMenuList)を置く。
// 選んだステージに合わせて、カードの中身と背景(StageDefinition::backdrop)を切り替える。
// 一覧から 1 つを決定したら、そのステージの ID を荷物(StageLaunchParams)に入れて、ゲームへ切り替える。
// 責務は「カーソルの移動、表示の切り替え、決定、ゲームへの引き渡し」。ステージの内容は StageDefinition、
// 一覧の読み込みは StageCatalog が持つ(このシーンは、それを読むだけ)。
// 操作の受け付けと画面の表示は ImGui に依存しない(配布ビルドでも動く)。ImGui の一覧は、開発ビルドの補助の表示(画面の左上に出す)。
class StageSelectScene final : public BaseScene {
public:
  StageSelectScene() = default;
  ~StageSelectScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

protected:
  // ステージ選択はレール演出を持たないため、タイムライン機能は不要
  bool UsesTimeline() const override { return false; }

private:
  // 展示: 操作を受け付けず、覆いが晴れてから一定時間たったら、カーソルのステージ(先頭)を自動で決定する
  void UpdateExhibition_();
  // メニューの入力(カーソルの移動と決定)を受け付ける。キーとパッドの読み取りは UiMenuList に任せる。遷移の最中は受け付けない
  void UpdateInput_();
  // 選んでいるステージが変わっていたら、カードの中身と背景を切り替える
  void SyncSelection_();
  // 背景を、ステージの世界観に合わせて切り替える
  void ApplyBackdrop_(const StageDefinition &stage);
  // 画面の配置を決める(カード、一覧のパネルの位置と大きさ。一覧の項目の数に合わせる)
  void LayoutPanels_();
  // カーソルのステージを決定する。解放済みなら、荷物をつけてゲームへ切り替える。ロック中なら何もしない
  void DecideCursorStage_();

  float exhibitionHoldTimer_ = 0.0f; // 展示: 覆いが晴れてからの経過(秒)
  ResultBackdrop backdrop_; // 背景の空(遷移の覆いが描画を通るようにもする)
  BitmapFont font_;         // UI の文字(日本語可)
  StageCard card_;          // 中央のカード(選んだステージの紹介)
  UiPanel listPanel_;       // 一覧の外枠(カードの下)
  UiMenuList menu_;         // ステージの一覧(選択位置は menu_.GetCursor。StageCatalog::GetStages の添字と同じ並び)
  int shownCursor_ = -1;    // カードと背景に反映済みの選択位置(-1 は未反映)
};
