#include "GameOverScene.h"
#include "ExhibitionMode.h"
#include "Input.h"
#include "SceneIds.h"
#include "SceneManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 背景の時間を進める 1 フレームの長さ
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;
// 見出しの画像と、操作の案内の画像
constexpr const char *kBannerTexturePath = "resources/app/result/gameOver.png";
constexpr const char *kPromptTexturePath = "resources/app/result/gameOver_prompt.png";

// 見出しの配置と、現れる区間(遷移の晴れる進み具合 0〜1。晴れ始めの霧の向こうから、先に見出しが現れる)
constexpr ResultBanner::Layout kBannerLayout{720.0f, 180.0f, 0.42f, 0.25f, 0.65f};
// 案内の配置(元画像は 4:1)と、現れる区間(見出しのあとに、霧がほぼ晴れた頃に現れる)
constexpr ResultBanner::Layout kPromptLayout{600.0f, 150.0f, 0.72f, 0.60f, 0.95f};

#ifdef USE_IMGUI
// デバッグ窓の位置と、ボタンの大きさ
constexpr float kDebugWindowX = 10.0f;
constexpr float kDebugWindowY = 10.0f;
constexpr float kDebugButtonWidth = 80.0f;
constexpr float kDebugButtonHeight = 28.0f;
#endif
} // namespace

void GameOverScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  backdrop_.Initialize();
  banner_.Initialize(kBannerTexturePath, kBannerLayout);
  prompt_.Initialize(kPromptTexturePath, kPromptLayout);
}

void GameOverScene::Finalize() {}

void GameOverScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);

  // 見出しと案内は、遷移の覆いが晴れる進み具合に合わせて、霧の向こうから現れる(遷移が無ければ、最初から見える)
  const float revealProgress = GetTransitionRevealProgress();
  banner_.SetRevealProgress(revealProgress);
  prompt_.SetRevealProgress(revealProgress);

  if (ExhibitionMode::kEnabled) {
    UpdateExhibition_();
  } else {
    UpdateInput_();
  }

#ifdef USE_IMGUI
  if (!ExhibitionMode::kEnabled) {
    DrawDebugWindow_();
  }
#endif
}

void GameOverScene::UpdateExhibition_() {
  // 遷移の覆いが晴れてから一定時間たったら、ゲームへ戻る(タイトルは通らない)
  if (IsSceneTransitioning()) return;
  exhibitionHoldTimer_ += kFrameDeltaSeconds;
  if (exhibitionHoldTimer_ >= ExhibitionMode::kResultHoldSec) {
    RequestSceneChange(SceneId::Game);
  }
}

void GameOverScene::UpdateInput_() {
  // 遷移の最中は受け付けない(死ぬ瞬間に押していたボタンで、すぐに抜けてしまうのを防ぐ)
  if (!GetInput() || IsSceneTransitioning()) return;
  if (GetInput()->TriggerKey(DIK_SPACE) || GetInput()->TriggerKey(DIK_RETURN) || GetInput()->WasPadPressed(XINPUT_GAMEPAD_A)) {
    RequestSceneChange(SceneId::Title);
  }
  if (GetInput()->TriggerKey(DIK_R) || GetInput()->WasPadPressed(XINPUT_GAMEPAD_B)) {
    RequestSceneChange(SceneId::Game);
  }
}

#ifdef USE_IMGUI
void GameOverScene::DrawDebugWindow_() {
  ImGui::SetNextWindowPos(ImVec2(kDebugWindowX, kDebugWindowY), ImGuiCond_Always);
  ImGui::Begin("Game Over Debug", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
  if (ImGui::Button("Title", ImVec2(kDebugButtonWidth, kDebugButtonHeight))) {
    RequestSceneChange(SceneId::Title);
  }
  ImGui::SameLine();
  if (ImGui::Button("Retry", ImVec2(kDebugButtonWidth, kDebugButtonHeight))) {
    RequestSceneChange(SceneId::Game);
  }
  ImGui::End();
}
#endif

void GameOverScene::Draw() {
  backdrop_.Begin();
  // 雲を先に合成し、その上に見出しと案内を描く(逆だと、雲が見出しを隠す)
  backdrop_.ResolveClouds();
  banner_.Draw();
  // 展示中は操作を受け付けないので、案内は出さない
  if (!ExhibitionMode::kEnabled) {
    prompt_.Draw();
  }
  backdrop_.End();
}
