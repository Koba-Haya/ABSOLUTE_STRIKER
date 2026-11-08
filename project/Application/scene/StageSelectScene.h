#pragma once
#include "BaseScene.h"
#include "ResultBackdrop.h"

// ステージ選択のシーン。ステージの一覧(StageCatalog)から 1 つを選び、選んだステージの ID を荷物(StageLaunchParams)に入れて、
// ゲームへ切り替える。責務は「カーソルの移動、決定、ゲームへの引き渡し」だけ。
// ステージの内容は StageDefinition、一覧の読み込みは StageCatalog が持つ(このシーンは、それを読むだけ)。
// 操作の受け付けは ImGui に依存しない(配布ビルドでも動く)。ImGui の一覧は、開発ビルドの確認用の表示。
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
  // キーとパッドの入力を受け付ける(カーソルの移動と決定)。遷移の最中は受け付けない
  void UpdateInput_();
  // カーソルを step だけ動かす(正で下、負で上)。端を越えたら反対側へ回り込む
  void MoveCursor_(int step);
  // カーソルのステージを決定する。解放済みなら、荷物をつけてゲームへ切り替える。ロック中なら何もしない
  void DecideCursorStage_();

  float exhibitionHoldTimer_ = 0.0f; // 展示: 覆いが晴れてからの経過(秒)
  int cursor_ = 0;          // 選択中のステージの位置(StageCatalog::GetStages の添字)
  ResultBackdrop backdrop_; // 背景の空(遷移の覆いが描画を通るようにもする)
};
