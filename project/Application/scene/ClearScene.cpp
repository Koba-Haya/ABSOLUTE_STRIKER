#include "ClearScene.h"
#include "Input.h"
#include "SceneIds.h"
#include "ExhibitionMode.h"
#include "SceneManager.h"
#include "ui/UiMenuInput.h"
#include "ui/UiPointer.h"
#include "../exhibition/ExhibitionHint.h"
#include "../ScreenSize.h"
#include "Logger.h"
#include "../stage/StageLaunchParams.h"
#include "../stage/StageProgress.h"
#include "../inheritance/InheritanceState.h"
#include "../item/ItemCatalog.h"
#include "../result/RatingCatalog.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 背景の注視点の高さ(正で空を見上げ、太陽が画面に入る)
constexpr float kBackdropTargetHeight = 0.30f;
// 背景の時間を進める 1 フレームの長さ
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;
// 見出しの画像
constexpr const char *kBannerTexturePath = "resources/app/result/gameClear.png";
// メニューが現れ始めるまでの時間(秒。シーンの開始から)。見出しが完全に現れる(ResultBanner の遅れ 0.4 + 0.8 = 1.2 秒)のを待ってから出す
constexpr float kMenuOpenDelaySec = 1.4f;
// 見出しの配置(結果の集計を置くため、画面の上寄りに、小さめに出す。画像は 4:1)
constexpr float kBannerWidth = 520.0f;
constexpr float kBannerHeight = 130.0f;
constexpr float kBannerCenterYRatio = 0.115f;
// 結果の集計は、見出しの下から、メニューの上までに置く。それぞれとの間隔
constexpr float kTallyGapAboveMenu = 16.0f;
constexpr float kTallyGapBelowBanner = 8.0f;
// 結果の集計が始まる(パネルが開き始める)までの待ち。見出しが現れる(約 1.2 秒)のを待つ
constexpr float kTallyDelaySec = 1.0f;
// 結果の集計があるとき、メニューを小さくする倍率(集計に、縦の余裕を回すため。結果が無いときは、原寸)
constexpr float kMenuScaleWithResult = 0.7f;
// 集計から継承の選択へ切り替えるとき、集計が薄れる時間(秒)。選択のパネルは、この間に開き始める
constexpr float kTallyFadeSeconds = 0.35f;
// 集計が終わったあと、継承へ進む入力を待つ間に出す案内の文字と位置(メニューは、継承が決まるまで開かないので、その場所を使う)
constexpr const char *kContinuePromptText = "決定ボタンかクリックで 継承へ";
constexpr float kContinuePromptOffsetBelowMenuTop = 30.0f; // メニューのパネルの上端から、案内の中心までの距離
} // namespace

void ClearScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  // ゲームから渡された、今回のプレイの結果(結果画面を作るまでの確認用に、ログへ出す)
  if (const auto *params = TryGetLaunchPayload<StageLaunchParams>()) {
    if (params->result) {
      Logger::GetInstance().Log("[Result] received: stage=%s rank=%s total=%.1f score=%d", params->stageId.c_str(),
                                RankToString(params->result->rank), params->result->totalPoints, params->result->score);
    } else {
      Logger::GetInstance().Log("[Result] no result in the payload (stage=%s)", params->stageId.c_str());
    }
  }
  backdrop_.Initialize(kBackdropTargetHeight);
  // 結果が渡されたときは、見出しを上寄りに小さく出して、その下に結果の集計を置く。結果が無いときは、従来の位置
  const auto *params = TryGetLaunchPayload<StageLaunchParams>();
  const bool hasResult = params && params->result;
  ResultBanner::Layout bannerLayout;
  if (hasResult) {
    bannerLayout.width = kBannerWidth;
    bannerLayout.height = kBannerHeight;
    bannerLayout.centerYRatio = kBannerCenterYRatio;
  }
  banner_.Initialize(kBannerTexturePath, bannerLayout);
  menu_.Initialize(hasResult ? kMenuScaleWithResult : 1.0f);
  hasResult_ = hasResult;
  if (hasResult) {
    resultRank_ = params->result->rank;
    // 集計の縦の範囲: 見出しの下から、メニューの上まで(継承の選択も、同じ範囲)
    const float bannerBottom = ScreenSize::kHeight * kBannerCenterYRatio + kBannerHeight * 0.5f;
    const float rangeTop = bannerBottom + kTallyGapBelowBanner;
    const float rangeBottom = menu_.GetPanelTop() - kTallyGapAboveMenu;
    tally_.Initialize();
    tally_.SetVerticalRange(rangeTop, rangeBottom);
    picker_.Initialize();
    picker_.SetVerticalRange(rangeTop, rangeBottom);
    continuePrompt_.Initialize();
    continuePrompt_.SetPosition(ScreenSize::kCenterX, menu_.GetPanelTop() + kContinuePromptOffsetBelowMenuTop);
    // 今回の結果を、ベスト記録と比べて、更新して保存する(集計を飛ばしても、記録は残る)。更新の内容は、集計が「ベスト更新」として見せる
    const BestUpdate bestUpdate = StageProgress::GetInstance().RecordRun(params->stageId, *params->result);
    Logger::GetInstance().Log("[Best] stage=%s previous=%s%s scoreImproved=%d pointsImproved=%d rankImproved=%d", params->stageId.c_str(),
                              bestUpdate.previous.exists ? "yes " : "none", bestUpdate.previous.exists ? RankToString(bestUpdate.previous.rank) : "",
                              bestUpdate.scoreImproved, bestUpdate.pointsImproved, bestUpdate.rankImproved);
    tally_.Begin(*params->result, bestUpdate, kTallyDelaySec);
  }
  // メニューは、結果の集計があるときは、集計が終わってから開く(Update で)。無いときは、従来どおり、最初から待ちつきで開く。
  // 展示中は、継承が決まってから(結果があるとき)開く。結果が無いときの、最初からの待ちつきの表示は、通常のときだけ
  if (!ExhibitionMode::IsEnabled() && !hasResult) {
    menu_.Open(kMenuOpenDelaySec);
    menuOpened_ = true;
  }
}

void ClearScene::Finalize() {}

void ClearScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  banner_.Update(kFrameDeltaSeconds);
  tally_.Update(kFrameDeltaSeconds);

  continuePrompt_.Update(kFrameDeltaSeconds);

  // 決定ボタンは、1 フレームに 1 回だけ読む(遷移の最中は受け付けない)。押した瞬間だけ反応するので、
  // 集計を飛ばした同じ押下で、継承へ進んでしまうことはない(下で、飛ばした押下は使い切ったものとして扱う)
  // マウスの左クリックも、決定と同じに扱う(メニューを、マウスでも操作できるようにするため)
  // 展示中は、操作を受け付けない(来た人が押したキーやクリックで、自動の進行が乱れないように)
  const bool exhibition = ExhibitionMode::IsEnabled();
  const UiPointer::State pointer = UiPointer::Poll(GetInput());
  const bool decidePressed = !exhibition && !IsSceneTransitioning() &&
                             (UiMenuInput::Poll(GetInput()) == UiMenuInput::Command::Decide || (pointer.valid && pointer.leftClicked));
  bool decideConsumed = false;
  // 結果の集計の最中に決定ボタンが押されたら、集計を飛ばす
  if (tally_.IsActive() && !tally_.IsFinished() && decidePressed) {
    tally_.Skip();
    decideConsumed = true;
  }
  // 集計が終わったら、継承の段階へ進む。
  // 今回取った強化が無ければ、選ぶものが無いので、すぐ確定へ進む。
  // 強化があれば、案内を出して、決定ボタンを待つ(結果と評価を、ゆっくり見せるため)。押されたら、継承の選択を始め、集計を薄く消す。
  // 展示中は、一定時間待ってから、自動操作で継承の選択を始める(仮の状態なので、保存されない)
  if (hasResult_ && tally_.IsFinished() && !inheritanceStarted_) {
    if (!InheritancePicker::HasCandidates()) {
      inheritanceStarted_ = true;
    } else if (exhibition) {
      exhibitionTallyTimer_ += kFrameDeltaSeconds;
      if (exhibitionTallyTimer_ >= ExhibitionMode::kTallyHoldSec) {
        inheritanceStarted_ = true;
        picker_.Begin(resultRank_, kTallyFadeSeconds);
        picker_.BeginAutoPlay(ExhibitionMode::kPickStepSec, ExhibitionMode::kPickMaxPerRow);
      }
    } else {
      continuePrompt_.Show(kContinuePromptText);
      if (decidePressed && !decideConsumed) {
        inheritanceStarted_ = true;
        continuePrompt_.Hide();
        picker_.Begin(resultRank_, kTallyFadeSeconds);
      }
    }
  }
  // 継承の選択が始まったら、集計を薄く消す
  if (picker_.IsActive() && tallyFadeSeconds_ < kTallyFadeSeconds) {
    tallyFadeSeconds_ += kFrameDeltaSeconds;
    tally_.SetOpacityScale((std::max)(0.0f, 1.0f - tallyFadeSeconds_ / kTallyFadeSeconds));
  }
  // 継承の選択の入力(展示中は、入力を読まず、自動操作を進める)
  picker_.UpdateAutoPlay(kFrameDeltaSeconds);
  picker_.Update(exhibition ? nullptr : GetInput(), !IsSceneTransitioning());
  // 継承を確定して(1 回だけ)、メニューを開く。選ぶものが無いとき(picker_ が動いていないとき)は、選ばずに確定する(世代だけ進む)
  if (inheritanceStarted_ && !inheritanceCommitted_ && (!picker_.IsActive() || picker_.IsDone())) {
    inheritanceCommitted_ = true;
    const std::vector<InheritanceState::Pick> picks = picker_.IsActive() ? picker_.GetPicks() : std::vector<InheritanceState::Pick>{};
    const float rarity = RatingCatalog::GetInstance().GetSettings().GetInheritanceRule(resultRank_).rarity;
    InheritanceState::GetInstance().Commit(picks, rarity, ItemCatalog::GetInstance().GetUpgradeSettings());
    menu_.Open();
    menuOpened_ = true;
  }

  if (exhibition) {
    // 展示: 操作を受け付けず、継承が決まって(結果が無いときは、集計が終わって)から一定時間たったら、ステージ選択へ進む
    // (ステージ選択で、解放された次のステージの鍵が外れる演出を見せる)
    const bool settled = hasResult_ ? inheritanceCommitted_ : (!tally_.IsActive() || tally_.IsFinished());
    if (!IsSceneTransitioning() && settled) {
      exhibitionHoldTimer_ += kFrameDeltaSeconds;
      if (exhibitionHoldTimer_ >= ExhibitionMode::kResultHoldSec) {
        RequestSceneChangeKeepingPayload(SceneId::StageSelect);
      }
    }
  } else {
    // 決定は、メニューの 1 つのボタンだけ(Space / Enter / パッド A)。遷移の最中は受け付けない
    // (前のシーンで押していたボタンで、すぐに決定してしまうのを防ぐ)
    switch (menu_.Update(GetInput(), !IsSceneTransitioning())) {
    case ResultMenu::Choice::Retry:
      RequestSceneChangeKeepingPayload(SceneId::Game);
      break;
    case ResultMenu::Choice::StageSelect:
      // 選んでいたステージにカーソルを合わせる
      RequestSceneChangeKeepingPayload(SceneId::StageSelect);
      break;
    case ResultMenu::Choice::Title:
      RequestSceneChange(SceneId::Title);
      break;
    case ResultMenu::Choice::None:
      break;
    }
  }

#ifdef USE_IMGUI
  ImGui::SetNextWindowPos(ImVec2(640, 360), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::Begin("Game Clear", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
  
  ImGui::SetWindowFontScale(3.0f);
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "GAME CLEAR!!");
  ImGui::SetWindowFontScale(1.0f);
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button("Title", ImVec2(120, 40))) {
    RequestSceneChange(SceneId::Title);
  }
  ImGui::SameLine();
  if (ImGui::Button("Retry", ImVec2(120, 40))) {
    RequestSceneChangeKeepingPayload(SceneId::Game);
  }
  ImGui::SameLine();
  if (ImGui::Button("Stage Select", ImVec2(120, 40))) {
    RequestSceneChangeKeepingPayload(SceneId::StageSelect);
  }

  ImGui::End();
#endif
}

void ClearScene::Draw() {
  backdrop_.Begin();
  banner_.Draw();
  tally_.Draw();
  continuePrompt_.Draw();
  picker_.Draw();
  menu_.Draw();
  ExhibitionHint::GetInstance().Draw();
  backdrop_.End();
}
