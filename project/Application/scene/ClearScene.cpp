#include "ClearScene.h"
#include "Input.h"
#include "SceneIds.h"
#include "ExhibitionMode.h"
#include "SceneManager.h"

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
} // namespace

void ClearScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  backdrop_.Initialize(kBackdropTargetHeight);
  banner_.Initialize(kBannerTexturePath);
  // 展示中は操作を受け付けないので、メニューは出さない
  menu_.Initialize();
  if (!ExhibitionMode::kEnabled) {
    menu_.Open(kMenuOpenDelaySec);
  }
}

void ClearScene::Finalize() {}

void ClearScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  banner_.Update(kFrameDeltaSeconds);

  if (ExhibitionMode::kEnabled) {
    // 展示: 操作を受け付けず、遷移の覆いが晴れてから一定時間たったらタイトルへ戻る
    const bool isTransitioning = IsSceneTransitioning();
    if (!isTransitioning) {
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
  menu_.Draw();
  backdrop_.End();
}
