#include "ClearScene.h"
#include "Input.h"
#include "SceneIds.h"
#include "ExhibitionMode.h"
#include "SceneManager.h"
#include "ui/UiMenuInput.h"
#include "../ScreenSize.h"
#include "Logger.h"
#include "../stage/StageLaunchParams.h"
#include "../stage/StageProgress.h"

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
    // 集計の縦の範囲: 見出しの下から、メニューの上まで
    const float bannerBottom = ScreenSize::kHeight * kBannerCenterYRatio + kBannerHeight * 0.5f;
    tally_.Initialize();
    tally_.SetVerticalRange(bannerBottom + kTallyGapBelowBanner, menu_.GetPanelTop() - kTallyGapAboveMenu);
    // 今回の結果を、ベスト記録と比べて、更新して保存する(集計を飛ばしても、記録は残る)。更新の内容は、集計が「ベスト更新」として見せる
    const BestUpdate bestUpdate = StageProgress::GetInstance().RecordRun(params->stageId, *params->result);
    Logger::GetInstance().Log("[Best] stage=%s previous=%s%s scoreImproved=%d pointsImproved=%d rankImproved=%d", params->stageId.c_str(),
                              bestUpdate.previous.exists ? "yes " : "none", bestUpdate.previous.exists ? RankToString(bestUpdate.previous.rank) : "",
                              bestUpdate.scoreImproved, bestUpdate.pointsImproved, bestUpdate.rankImproved);
    tally_.Begin(*params->result, bestUpdate, kTallyDelaySec);
  }
  // メニューは、結果の集計があるときは、集計が終わってから開く(Update で)。無いときは、従来どおり、最初から待ちつきで開く。
  // 展示中は操作を受け付けないので、メニューは出さない
  if (!ExhibitionMode::kEnabled && !hasResult) {
    menu_.Open(kMenuOpenDelaySec);
    menuOpened_ = true;
  }
}

void ClearScene::Finalize() {}

void ClearScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  banner_.Update(kFrameDeltaSeconds);
  tally_.Update(kFrameDeltaSeconds);

  // 結果の集計の最中に、決定ボタンが押されたら、集計を飛ばす(遷移の最中は受け付けない)。押した瞬間だけ反応するので、
  // 集計が終わって開いたメニューが、同じ押下で決定してしまうことはない
  if (tally_.IsActive() && !tally_.IsFinished() && !IsSceneTransitioning() &&
      UiMenuInput::Poll(GetInput()) == UiMenuInput::Command::Decide) {
    tally_.Skip();
  }
  // 集計が終わったら、メニューを開く(展示中は出さない)
  if (tally_.IsFinished() && !menuOpened_ && !ExhibitionMode::kEnabled) {
    menu_.Open();
    menuOpened_ = true;
  }

  if (ExhibitionMode::kEnabled) {
    // 展示: 操作を受け付けず、遷移の覆いが晴れてから(結果の集計があるときは、集計が終わってから)一定時間たったらタイトルへ戻る
    const bool isTransitioning = IsSceneTransitioning();
    const bool tallyDone = !tally_.IsActive() || tally_.IsFinished();
    if (!isTransitioning && tallyDone) {
      exhibitionHoldTimer_ += kFrameDeltaSeconds;
      if (exhibitionHoldTimer_ >= ExhibitionMode::kResultHoldSec) {
        RequestSceneChange(SceneId::Title);
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
  menu_.Draw();
  backdrop_.End();
}
