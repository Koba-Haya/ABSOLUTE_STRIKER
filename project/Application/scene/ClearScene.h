#pragma once
#include "BaseScene.h"
#include "ResultBackdrop.h"
#include "ResultBanner.h"
#include "ui/ResultMenu.h"
#include "ui/ResultTally.h"

class ClearScene final : public BaseScene {
public:
  ClearScene() = default;
  ~ClearScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

protected:
  // クリア画面はレール演出を持たないためタイムライン機能は不要
  bool UsesTimeline() const override { return false; }

private:
  float exhibitionHoldTimer_ = 0.0f; // 展示: 覆いが晴れてからの経過(秒)
  ResultBanner banner_;     // 見出し(スプライト)
  ResultMenu menu_;         // 結果メニュー(もう一度、ステージ選択へ、タイトルへ)
  ResultTally tally_;       // 結果の集計(評価の内訳、合計、ランク)。結果が渡されたときだけ使う
  bool menuOpened_ = false; // メニューを開き始めたか(集計があるときは、集計が終わってから開く)
  ResultBackdrop backdrop_; // 背景の空(遷移の覆いが描画を通るようにもする)
};
