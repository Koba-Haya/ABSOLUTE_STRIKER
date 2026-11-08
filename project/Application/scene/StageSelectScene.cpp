#include "StageSelectScene.h"
#include "Input.h"
#include "ExhibitionMode.h"
#include "SceneIds.h"
#include "../ScreenSize.h"
#include "../stage/StageCatalog.h"
#include "../stage/StageLaunchParams.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 背景の注視点の高さ(正で空を見上げ、太陽が画面に入る)。クリア画面と同じ空にそろえる
constexpr float kBackdropTargetHeight = 0.30f;
// 背景の時間を進める 1 フレームの長さ
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;
// カーソルの移動量
constexpr int kMoveUp = -1;
constexpr int kMoveDown = 1;

#ifdef USE_IMGUI
// 開発用の一覧の表示(ImGui)
constexpr float kListWindowWidth = 320.0f;
constexpr float kTitleFontScale = 2.0f;
constexpr float kEntryHeight = 40.0f;
#endif
} // namespace

void StageSelectScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  backdrop_.Initialize(kBackdropTargetHeight);

  // クリアやゲームオーバーから戻ったときは、遊んでいたステージにカーソルを合わせる(荷物が無い、または一覧に無ければ先頭のまま)
  if (const auto *params = TryGetLaunchPayload<StageLaunchParams>()) {
    const auto &stages = StageCatalog::GetInstance().GetStages();
    for (int i = 0; i < static_cast<int>(stages.size()); ++i) {
      if (stages[i].id == params->stageId) {
        cursor_ = i;
        break;
      }
    }
  }
}

void StageSelectScene::Finalize() {}

void StageSelectScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  if (ExhibitionMode::kEnabled) {
    UpdateExhibition_();
  } else {
    UpdateInput_();
  }

#ifdef USE_IMGUI
  // 開発用の一覧。ステージ選択の本番の画面は、UI の部品ができてから作る
  const auto &stages = StageCatalog::GetInstance().GetStages();
  ImGui::SetNextWindowPos(ImVec2(ScreenSize::kWidth * 0.5f, ScreenSize::kHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(kListWindowWidth, 0.0f));
  ImGui::Begin("Stage Select", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

  ImGui::SetWindowFontScale(kTitleFontScale);
  ImGui::Text("STAGE SELECT");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Separator();

  for (int i = 0; i < static_cast<int>(stages.size()); ++i) {
    const StageDefinition &stage = stages[i];
    const std::string label = stage.displayName + (stage.unlocked ? "" : "  [LOCKED]") + "##" + stage.id;
    if (!stage.unlocked) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Selectable(label.c_str(), i == cursor_, 0, ImVec2(0.0f, kEntryHeight))) {
      cursor_ = i;
      DecideCursorStage_();
    }
    if (!stage.unlocked) {
      ImGui::EndDisabled();
    }
  }

  ImGui::End();
#endif
}

void StageSelectScene::Draw() {
  backdrop_.Begin();
  backdrop_.End();
}

void StageSelectScene::UpdateExhibition_() {
  // 遷移の覆いが晴れてから数える(覆われている間に、決定してしまわないため)
  if (IsSceneTransitioning()) return;
  exhibitionHoldTimer_ += kFrameDeltaSeconds;
  if (exhibitionHoldTimer_ >= ExhibitionMode::kStageSelectHoldSec) {
    DecideCursorStage_();
  }
}

void StageSelectScene::UpdateInput_() {
  // 遷移の最中は受け付けない(前のシーンで押していたボタンで、すぐに決定してしまうのを防ぐ)
  if (!GetInput() || IsSceneTransitioning()) return;

  Input *input = GetInput();
  if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W) || input->WasPadPressed(XINPUT_GAMEPAD_DPAD_UP)) {
    MoveCursor_(kMoveUp);
  }
  if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S) || input->WasPadPressed(XINPUT_GAMEPAD_DPAD_DOWN)) {
    MoveCursor_(kMoveDown);
  }
  if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->WasPadPressed(XINPUT_GAMEPAD_A)) {
    DecideCursorStage_();
  }
}

void StageSelectScene::MoveCursor_(int step) {
  const int count = static_cast<int>(StageCatalog::GetInstance().GetStages().size());
  // 負の移動でも、0 未満にならないよう、count を足してから余りを取る
  cursor_ = (cursor_ + step % count + count) % count;
}

void StageSelectScene::DecideCursorStage_() {
  const StageDefinition &stage = StageCatalog::GetInstance().GetStages()[cursor_];
  if (!stage.unlocked) return;
  RequestSceneChange(SceneId::Game, StageLaunchParams{stage.id});
}
