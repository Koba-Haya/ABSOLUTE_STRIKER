#pragma once
#include "BaseScene.h"
#include "ResultBackdrop.h"
#include "ResultBanner.h"
#include "ui/ResultMenu.h"
#include "ui/InheritancePicker.h"
#include "ui/ResultTally.h"
#include "ui/UiContinuePrompt.h"
#include "result/RunResult.h"

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
  float exhibitionHoldTimer_ = 0.0f;  // 展示: 継承の確定(メニューが開いたあと)からの経過(秒)
  float exhibitionTallyTimer_ = 0.0f; // 展示: 結果の集計が終わってからの経過(秒。継承の選択を始めるまでの待ち)
  ResultBanner banner_;     // 見出し(スプライト)
  ResultMenu menu_;         // 結果メニュー(もう一度、ステージ選択へ、タイトルへ)
  ResultTally tally_;       // 結果の集計(評価の内訳、合計、ランク)。結果が渡されたときだけ使う
  InheritancePicker picker_; // 継承の選択(集計のあと、今回取った強化があるときだけ)
  UiContinuePrompt continuePrompt_; // 集計のあと、「決定ボタンで継承へ」の案内(結果をゆっくり見せるため、入力を待つ)
  bool menuOpened_ = false; // メニューを開き始めたか(集計があるときは、継承が決まってから開く)
  bool hasResult_ = false;  // 今回のプレイの結果が渡されたか(クリアの結果があるときだけ、継承の選択と確定を行う)
  RunRank resultRank_ = RunRank::C; // 今回の結果のランク(継承の枠と倍率が決まる)
  bool inheritanceStarted_ = false;   // 集計が終わって、継承の段階へ進んだか
  bool inheritanceCommitted_ = false; // 継承を確定したか(1 回だけ)
  float tallyFadeSeconds_ = 0.0f;     // 継承の選択が始まってからの経過(集計を薄く消すために使う)
  ResultBackdrop backdrop_; // 背景の空(遷移の覆いが描画を通るようにもする)
};
