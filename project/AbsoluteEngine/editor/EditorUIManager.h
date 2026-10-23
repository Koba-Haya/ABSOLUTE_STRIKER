#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../Type/Matrix.h"
#include "../Type/Vector.h"
#include "../Type/Transform.h"
#include "CommandManager.h"
#include "../../externals/nlohmann/json.hpp"
#include "../scene/LightNodeComponent.h"
// タイムラインマネージャーとのインスペクタ連携（タスク15）
#include "../scene/timeline/TimelineManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include "../../externals/ImGuizmo/ImGuizmo.h"
#endif

namespace AbsoluteEngine {

class GameObject;
class EditorCamera;

class EditorUIManager {
public:
  EditorUIManager();
  ~EditorUIManager() = default;

  // 毎フレームのUI描画
  void DrawUI(std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, EditorCamera* camera = nullptr);

  // エディタ用カメラの位置・回転を編集する入力欄を描画する（呼び出し側の ImGui ウィンドウの中に並べる）
  // 状態を持たないため static。USE_IMGUI が無いビルドでは何もしない
  static void DrawCameraControls(EditorCamera& camera);

  // Viewportへのドラッグ＆ドロップ受付（画像描画後に手動で呼ぶ）
  void HandleViewportDragDrop(std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);

  // 現在選択されているオブジェクトを取得
  std::shared_ptr<GameObject> GetSelectedObject() const { return selectedObject_.lock(); }
  void SetSelectedObject(std::shared_ptr<GameObject> obj) { selectedObject_ = obj; }

  // タイムラインマネージャへの非所有ポインタを設定する（タスク15: インスペクタ連携）
  // BaseScene::DrawEditorUI から呼び出してボイントする
  void SetTimelineManager(AbsoluteEngine::TimelineManager* manager) { timelineManager_ = manager; }

  // シーンが変更されたかどうかを取得・クリアする
  bool ConsumeSceneModifiedFlag() {
      bool flag = isSceneModified_;
      isSceneModified_ = false;
      return flag;
  }
  void SetSceneModified() { isSceneModified_ = true; }

  CommandManager* GetCommandManager() const { return commandManager_.get(); }

private:
#ifdef USE_IMGUI
  void DrawMenuBar(std::vector<std::shared_ptr<GameObject>>& rootObjects);
  void DrawToolbar();
  void DrawAssetBrowser(std::vector<std::shared_ptr<GameObject>>& rootObjects);
  void DrawPrefabsBrowser(std::vector<std::shared_ptr<GameObject>>& rootObjects);
  void DrawHierarchy(std::vector<std::shared_ptr<GameObject>>& rootObjects);
  void DrawGameObjectNode(std::shared_ptr<GameObject> obj, std::vector<std::shared_ptr<GameObject>>& rootObjects);
  void DrawInspector();
  void DrawViewport(std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
  void DrawPostProcessSettings();
  void HandleShortcuts(std::vector<std::shared_ptr<GameObject>>& rootObjects, EditorCamera* camera);
  
  void HandleMousePicking(const std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
  void CheckIntersection(std::shared_ptr<GameObject> obj, const Vector3& rayOrigin, const Vector3& rayDir, std::shared_ptr<GameObject>& hitObject, float& minT);
  void DrawColliderDebug(const std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const ImVec2& vMin, const ImVec2& vMax);

  // タイムラインイベント選択時のインスペクタ専用UI（タスク15）
  void DrawTimelineEventInspector();

  // オブジェクト選択の一元窓口。selectedObject_を更新するのと同時に、
  // タイムラインイベント選択（timelineManager_->GetSelectedEvent()）をクリアする。
  // DrawInspector()はイベント選択を優先して早期リターンするため、これを経由せずに
  // selectedObject_だけを更新すると、Hierarchy/Viewportでオブジェクトを選び直しても
  // インスペクタがタイムラインイベント側に固定されたまま切り替わらないバグになる。
  void SelectObject(const std::shared_ptr<GameObject>& obj);

  // Post process params
  int postProcessMode_ = 0;
  float vignetteScale_ = 16.0f;
  float vignettePow_ = 0.8f;
  int boxFilterK_ = 1;
  int gaussianFilterK_ = 3;
  float gaussianFilterSigma_ = 1.0f;
  float dissolveThreshold_ = 0.0f;
  Vector3 dissolveEdgeColor_ = {1.0f, 0.0f, 0.0f};
  float dissolveEdgeRange_ = 0.1f;
#endif

  std::weak_ptr<GameObject> selectedObject_;
  std::shared_ptr<GameObject> clipboardObject_; // コピー＆ペースト用のバッファ
  std::unique_ptr<CommandManager> commandManager_ = std::make_unique<CommandManager>();

#ifdef USE_IMGUI
  ImGuizmo::OPERATION currentGizmoOperation_ = ImGuizmo::TRANSLATE;
  bool useSnap_ = false;
  float snapValue_ = 1.0f;
  float dropDistance_ = 20.0f; // ドラッグ＆ドロップ時のカメラからの距離
  bool showColliderDebug_ = true;
  
  bool isGizmoUsing_ = false;
  Transform transformBeforeGizmo_;

  Transform transformBeforeInspector_;
  nlohmann::json componentStateBefore_;

  // タイムラインイベントインスペクタ編集用のUndo/Redoスナップショット（タスクF）
  // IsItemActivated()時に取得し、IsItemDeactivatedAfterEdit()時にTimelineCommandを発行する
  std::string timelineSnapshotBeforeEdit_;
#endif
  bool isSceneModified_ = false;

  // タイムラインマネージャへの非所有ポインタ（タスク15）
  // BaseScene で初期化時に SetTimelineManager() でボイントする
  AbsoluteEngine::TimelineManager* timelineManager_ = nullptr;
};

} // namespace AbsoluteEngine
