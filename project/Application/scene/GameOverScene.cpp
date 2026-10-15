#include "GameOverScene.h"
#include "Input.h"
#include "SceneIds.h"
#include "ExhibitionMode.h"
#include "SceneManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 背景の注視点の高さ(負で地平線の下を見下ろし、暗く霞んだ色になる)
constexpr float kBackdropTargetHeight = -0.45f;
// 背景の時間を進める 1 フレームの長さ
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;
// 見出しの画像
constexpr const char *kBannerTexturePath = "resources/app/result/gameOver.png";
} // namespace

void GameOverScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  backdrop_.Initialize(kBackdropTargetHeight);
  banner_.Initialize(kBannerTexturePath);
}

void GameOverScene::Finalize() {}

void GameOverScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  banner_.Update(kFrameDeltaSeconds);

  if (ExhibitionMode::kEnabled) {
    // 展示: 操作を受け付けず、遷移の覆いが晴れてから一定時間たったらタイトルへ戻る
    const bool isTransitioning = sceneManager_ != nullptr && sceneManager_->IsTransitioning();
    if (!isTransitioning) {
      exhibitionHoldTimer_ += kFrameDeltaSeconds;
      if (exhibitionHoldTimer_ >= ExhibitionMode::kResultHoldSec) {
        RequestSceneChange(SceneId::Title);
      }
    }
  } else if (services_.input) {
    if (services_.input->TriggerKey(DIK_SPACE) || services_.input->TriggerKey(DIK_RETURN) || services_.input->WasPadPressed(XINPUT_GAMEPAD_A)) {
      RequestSceneChange(SceneId::Title);
    }
    if (services_.input->TriggerKey(DIK_R) || services_.input->WasPadPressed(XINPUT_GAMEPAD_B)) {
      RequestSceneChange(SceneId::Game);
    }
  }

#ifdef USE_IMGUI
  ImGui::SetNextWindowPos(ImVec2(640, 360), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::Begin("Game Over", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
  
  ImGui::SetWindowFontScale(3.0f);
  ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "GAME OVER...");
  ImGui::SetWindowFontScale(1.0f);
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button("Title", ImVec2(120, 40))) {
    RequestSceneChange(SceneId::Title);
  }
  ImGui::SameLine();
  if (ImGui::Button("Retry", ImVec2(120, 40))) {
    RequestSceneChange(SceneId::Game);
  }

  ImGui::End();
#endif
}

void GameOverScene::Draw() {
  backdrop_.Begin();
  banner_.Draw();
  backdrop_.End();
}
