#pragma once
#include "BaseScene.h"
#include "ResultBackdrop.h"
#include "ResultBanner.h"
#include "ui/ResultMenu.h"
#include "ui/InheritancePicker.h"
#include "ui/ResultTally.h"
#include "ui/UiContinuePrompt.h"
#include "result/RunResult.h"
#include "resultflow/IResultFlowActions.h"
#include "resultflow/IResultFlowState.h"
#include <memory>

class ExhibitionHint;
class GameData;
class ExhibitionSession;

// クリア画面。結果の集計 -> 継承の選択 -> メニュー の段階は、状態クラス(resultflow/ の ResultTallyState など。基底は IResultFlowState)が決める。
// このシーンは、画面の部品(集計、継承の選択、案内、メニュー)を持って毎フレーム動かし、段階の状態クラスへ窓口(IResultFlowActions)を渡すだけ。
// IResultFlowActions は private で継承する(窓口として渡せるのは、このシーン自身だけ。GameScene の IGameFlowActions と同じ形)
class ClearScene final : public BaseScene, private IResultFlowActions {
public:
  // exhibition: 展示モードの状態(読むだけ。所有しない。シーンより長く生きること)
  // exhibitionHint: 展示モードの切り替えの案内(描画に使う。所有しない)
  // gameData: ゲームのデータ(定義の表と、進行・継承の保存データ。所有しない。シーンより長く生きること)
  ClearScene(const ExhibitionSession &exhibition, ExhibitionHint &exhibitionHint, GameData &gameData)
      : exhibition_(exhibition), exhibitionHint_(exhibitionHint), gameData_(gameData) {}
  ~ClearScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

protected:
  // クリア画面はレール演出を持たないためタイムライン機能は不要
  bool UsesTimeline() const override { return false; }

private:
  // --- 段階の状態クラスへの窓口(IResultFlowActions の実装。意味は IResultFlowActions.h を参照) ---
  bool IsDecidePressed() const override { return decidePressed_; }
  bool IsExhibition() const override;
  bool IsTransitioning() const override { return IsSceneTransitioning(); }
  bool IsTallyFinished() const override { return tally_.IsFinished(); }
  void SkipTally() override { tally_.Skip(); }
  bool HasPickCandidates() const override;
  void ShowContinuePrompt() override;
  void HideContinuePrompt() override { continuePrompt_.Hide(); }
  void BeginPick(bool autoPlay) override;
  bool IsPickDone() const override { return picker_.IsDone(); }
  void CommitInheritance() override;
  void OpenMenu(float delaySeconds) override { menu_.Open(delaySeconds); }
  ResultMenu::Choice UpdateMenu() override;
  void RequestSceneFor(ResultMenu::Choice choice) override;

  // 決定(決定ボタンかクリック)が、このフレームに押されたかを読む(1 フレームに 1 回)。展示中と、画面の切り替えの最中は false
  bool PollDecide_() const;

  const ExhibitionSession &exhibition_; // 展示モードの状態(所有しない)
  ExhibitionHint &exhibitionHint_;      // 展示モードの切り替えの案内(所有しない)
  GameData &gameData_;                  // ゲームのデータ(所有しない)
  std::unique_ptr<IResultFlowState> flow_; // 今の段階(Initialize で決める。結果があれば集計、無ければメニューから)
  bool decidePressed_ = false;             // このフレームに、決定が押されたか(Update の始めに 1 回だけ読む)
  ResultBanner banner_;     // 見出し(スプライト)
  ResultMenu menu_;         // 結果メニュー(もう一度、ステージ選択へ、タイトルへ)
  ResultTally tally_;       // 結果の集計(評価の内訳、合計、ランク)。結果が渡されたときだけ使う
  InheritancePicker picker_; // 継承の選択(集計のあと、今回取った強化があるときだけ)
  UiContinuePrompt continuePrompt_; // 集計のあと、「決定ボタンで継承へ」の案内(結果をゆっくり見せるため、入力を待つ)
  RunRank resultRank_ = RunRank::C; // 今回の結果のランク(継承の枠と倍率が決まる)
  float tallyFadeSeconds_ = 0.0f;     // 継承の選択が始まってからの経過(集計を薄く消すために使う)
  ResultBackdrop backdrop_; // 背景の空(遷移の覆いが描画を通るようにもする)
};
