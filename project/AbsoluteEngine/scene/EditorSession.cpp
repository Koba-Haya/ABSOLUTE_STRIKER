#include "EditorSession.h"

#include "AbsoluteEngine/editor/EditorCamera.h"
#include "AbsoluteEngine/editor/EditorUIManager.h"
#include "SceneWorld.h"
#include "TimelineHost.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AbsoluteEngine {

EditorSession::EditorSession() : ui_(std::make_unique<EditorUIManager>()) {}

EditorSession::~EditorSession() = default;

void EditorSession::SetTimelineManager(TimelineManager *manager) { ui_->SetTimelineManager(manager); }

ToolbarAction EditorSession::DrawToolbar(PlayMode mode) {
  ToolbarAction action = ToolbarAction::None;
#ifdef USE_IMGUI
  // ツールバー(プレイモード切り替え)。押されたボタンを返すだけで、状態の変更は呼び出し側が行う
  ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);

  if (mode == PlayMode::Edit) {
    if (ImGui::Button("Play")) action = ToolbarAction::Play;
  } else if (mode == PlayMode::Play) {
    if (ImGui::Button("Pause")) action = ToolbarAction::Pause;
    ImGui::SameLine();
    if (ImGui::Button("Stop")) action = ToolbarAction::Stop;
  } else if (mode == PlayMode::Pause) {
    if (ImGui::Button("Resume")) action = ToolbarAction::Resume;
    ImGui::SameLine();
    if (ImGui::Button("Stop")) action = ToolbarAction::Stop;
  }
  ImGui::End();
#else
  (void)mode;
#endif
  return action;
}

void EditorSession::DrawPanels(SceneWorld &world, const Camera &viewCamera, EditorCamera *editorCamera) {
  // 「Viewport##GameView」の描画(ゲーム画面のプレビュー表示)はプレイ中も必要なため、
  // DrawUI() 自体は常に呼ぶ(編集モード限定にすると、プレイ中に編集ツール一式ごとゲーム画面まで消えてしまう)
  ui_->DrawUI(world, viewCamera.GetViewMatrix(), viewCamera.GetProjectionMatrix(), editorCamera);
}

bool EditorSession::ConsumeSceneModified() { return ui_->ConsumeSceneModifiedFlag(); }

void EditorSession::NotifySceneModified() { ui_->SetSceneModified(); }

void EditorSession::ClearSelection() { ui_->SetSelectedObject(nullptr); }

void EditorSession::UpdateTimelinePreview(TimelineHost &timeline, const TimelineSpawnPreview::AddObjectFunc &addObject) {
  // 仮のオブジェクトの追加は呼び出し側から受け取った関数を通す。選択はエディタ UI へ伝える
  timeline.UpdatePreview(addObject,
                         [this](const std::shared_ptr<GameObject> &obj) { ui_->SetSelectedObject(obj); });
}

bool EditorSession::IsDragDropInProgress() {
#ifdef USE_IMGUI
  return ImGui::GetDragDropPayload() != nullptr;
#else
  return false;
#endif
}

CommandManager *EditorSession::GetCommandManager() const { return ui_->GetCommandManager(); }

} // namespace AbsoluteEngine
