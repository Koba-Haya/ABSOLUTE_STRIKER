#include "ClearScene.h"
#include "Input.h"
#include "SceneIds.h"
#include "ExhibitionMode.h"
#include "SceneManager.h"
#include "ui/UiMenuInput.h"
#include "ui/UiPointer.h"
#include "../exhibition/ExhibitionHint.h"
#include "../exhibition/ExhibitionSession.h"
#include "../GameData.h"
#include "../ScreenSize.h"
#include "Logger.h"
#include "../stage/StageLaunchParams.h"
#include "../stage/StageProgress.h"
#include "../inheritance/InheritanceState.h"
#include "../item/ItemCatalog.h"
#include "../result/RatingCatalog.h"
#include "resultflow/ResultMenuState.h"
#include "resultflow/ResultTallyState.h"

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
  if (hasResult) {
    resultRank_ = params->result->rank;
    // 集計の縦の範囲: 見出しの下から、メニューの上まで(継承の選択も、同じ範囲)
    const float bannerBottom = ScreenSize::kHeight * kBannerCenterYRatio + kBannerHeight * 0.5f;
    const float rangeTop = bannerBottom + kTallyGapBelowBanner;
    const float rangeBottom = menu_.GetPanelTop() - kTallyGapAboveMenu;
    tally_.Initialize(gameData_.GetRatingCatalog());
    tally_.SetVerticalRange(rangeTop, rangeBottom);
    picker_.Initialize(gameData_.GetItemCatalog(), gameData_.GetRatingCatalog(), gameData_.GetInheritanceState(),
                      gameData_.GetPlayerUpgradeState());
    picker_.SetVerticalRange(rangeTop, rangeBottom);
    continuePrompt_.Initialize();
    continuePrompt_.SetPosition(ScreenSize::kCenterX, menu_.GetPanelTop() + kContinuePromptOffsetBelowMenuTop);
    // 今回の結果を、ベスト記録と比べて、更新して保存する(集計を飛ばしても、記録は残る)。更新の内容は、集計が「ベスト更新」として見せる
    const BestUpdate bestUpdate = gameData_.GetStageProgress().RecordRun(params->stageId, *params->result);
    Logger::GetInstance().Log("[Best] stage=%s previous=%s%s scoreImproved=%d pointsImproved=%d rankImproved=%d", params->stageId.c_str(),
                              bestUpdate.previous.exists ? "yes " : "none", bestUpdate.previous.exists ? RankToString(bestUpdate.previous.rank) : "",
                              bestUpdate.scoreImproved, bestUpdate.pointsImproved, bestUpdate.rankImproved);
    tally_.Begin(*params->result, bestUpdate, kTallyDelaySec);
  }
  // 最初の段階: 結果があれば集計から(メニューは、継承が決まってから開く)。無いときは、メニューから(最初から待ちつきで開く)。
  // 結果が無いまま展示で来たときは、メニューを開かない(時間でステージ選択へ進む)
  if (hasResult) {
    flow_ = std::make_unique<ResultTallyState>();
  } else {
    flow_ = std::make_unique<ResultMenuState>(exhibition_.IsEnabled() ? std::nullopt : std::optional<float>(kMenuOpenDelaySec));
  }
  flow_->Enter(*this);
}

void ClearScene::Finalize() {}

void ClearScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  banner_.Update(kFrameDeltaSeconds);
  tally_.Update(kFrameDeltaSeconds);

  continuePrompt_.Update(kFrameDeltaSeconds);

  // 決定ボタンは、1 フレームに 1 回だけ読む(段階の状態クラスが読む)。押した瞬間だけ反応するので、
  // 集計を飛ばした押下で、同じフレームに継承へ進んでしまうことはない
  const bool exhibition = exhibition_.IsEnabled();
  decidePressed_ = PollDecide_();

  // 継承の選択が始まったら、集計を薄く消す(選択のパネルが開くのと重ねる。段階に関係なく、消え切るまで進める)
  if (picker_.IsActive() && tallyFadeSeconds_ < kTallyFadeSeconds) {
    tallyFadeSeconds_ += kFrameDeltaSeconds;
    tally_.SetOpacityScale((std::max)(0.0f, 1.0f - tallyFadeSeconds_ / kTallyFadeSeconds));
  }
  // 継承の選択の入力(選択をしていない間は、何もしない。展示中は、入力を読まず、自動操作を進める)
  picker_.UpdateAutoPlay(kFrameDeltaSeconds);
  picker_.Update(exhibition ? nullptr : GetInput(), !IsSceneTransitioning());

  // 段階を 1 フレーム進める。次の段階は、段階のクラスが決める。切り替えは Update のあと(実行中の段階を消さないため)。
  // 継承の選択の入力のあとに進める(選び終わったフレームに、継承の確定とメニューを開くところまで進むため。以前と同じ順)
  if (std::unique_ptr<IResultFlowState> next = flow_->Update(*this, kFrameDeltaSeconds)) {
    flow_ = std::move(next);
    flow_->Enter(*this);
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

bool ClearScene::PollDecide_() const {
  // マウスの左クリックも、決定と同じに扱う(メニューを、マウスでも操作できるようにするため)。
  // 展示中は、操作を受け付けない(来た人が押したキーやクリックで、自動の進行が乱れないように)
  if (exhibition_.IsEnabled() || IsSceneTransitioning()) return false;
  const UiPointer::State pointer = UiPointer::Poll(GetInput());
  return UiMenuInput::Poll(GetInput()) == UiMenuInput::Command::Decide || (pointer.valid && pointer.leftClicked);
}

bool ClearScene::IsExhibition() const {
  return exhibition_.IsEnabled();
}

bool ClearScene::HasPickCandidates() const {
  return InheritancePicker::HasCandidates(gameData_.GetPlayerUpgradeState());
}

void ClearScene::ShowContinuePrompt() {
  continuePrompt_.Show(kContinuePromptText);
}

void ClearScene::BeginPick(bool autoPlay) {
  picker_.Begin(resultRank_, kTallyFadeSeconds);
  if (autoPlay) {
    picker_.BeginAutoPlay(ExhibitionMode::kPickStepSec, ExhibitionMode::kPickMaxPerRow);
  }
}

void ClearScene::CommitInheritance() {
  // 選択を始めていなければ(選ぶものが無いとき)、選ばずに確定する(世代だけ進む)
  const std::vector<InheritanceState::Pick> picks = picker_.IsActive() ? picker_.GetPicks() : std::vector<InheritanceState::Pick>{};
  const float rarity = gameData_.GetRatingCatalog().GetSettings().GetInheritanceRule(resultRank_).rarity;
  gameData_.GetInheritanceState().Commit(picks, rarity, gameData_.GetItemCatalog().GetUpgradeSettings());
}

ResultMenu::Choice ClearScene::UpdateMenu() {
  // 画面の切り替えの最中は受け付けない(前のシーンで押していたボタンで、すぐに決定してしまうのを防ぐ)
  return menu_.Update(GetInput(), !IsSceneTransitioning());
}

void ClearScene::RequestSceneFor(ResultMenu::Choice choice) {
  switch (choice) {
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
  default:
    break;
  }
}

void ClearScene::Draw() {
  backdrop_.Begin();
  banner_.Draw();
  tally_.Draw();
  continuePrompt_.Draw();
  picker_.Draw();
  menu_.Draw();
  exhibitionHint_.Draw(exhibition_.IsEnabled());
  backdrop_.End();
}
