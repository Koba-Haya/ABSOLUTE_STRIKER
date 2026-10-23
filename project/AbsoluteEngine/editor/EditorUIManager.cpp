#include "EditorUIManager.h"
#include "../scene/GameObject.h"
#include "../scene/SceneSerializer.h"
#include "../scene/ComponentFactory.h"
#include "EditorCamera.h"
#include "Method.h"
#include "../base/EnginePath.h"
#include <filesystem>
#include "../scene/ColliderComponent.h"
#include "../scene/LightNodeComponent.h"
#include "../scene/ModelComponent.h"
#include "../scene/DissolveComponent.h"
#include "../graphics/Renderer.h"
// タイムラインイベントインスペクタ用（タスク15）
#include "../scene/timeline/ITimelineEvent.h"
#include "../scene/timeline/PrefabRegistry.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "../../externals/ImGuizmo/ImGuizmo.h"
#endif

namespace AbsoluteEngine {

EditorUIManager::EditorUIManager() {
    if (commandManager_) {
        commandManager_->SetOnCommandExecutedCallback([this]() { SetSceneModified(); });
    }
}

void EditorUIManager::DrawUI(std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, EditorCamera* camera) {
#ifdef USE_IMGUI
  DrawMenuBar(rootObjects);
  DrawToolbar();
  DrawAssetBrowser(rootObjects);
  DrawPrefabsBrowser(rootObjects);
  DrawHierarchy(rootObjects);
  DrawInspector();
  DrawViewport(rootObjects, viewMatrix, projectionMatrix);
  DrawPostProcessSettings();
  HandleShortcuts(rootObjects, camera);
#endif
}

#ifdef USE_IMGUI
void EditorUIManager::DrawMenuBar(std::vector<std::shared_ptr<GameObject>>& rootObjects) {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      // カレントディレクトリ（実行ファイルの位置）に依存しないよう、プロジェクトフォルダへの絶対パスを指定
      std::string saveDir = AbsoluteEngine::EnginePath::Resolve("resources/editor/");

      if (ImGui::MenuItem("Save Scene")) {
        std::filesystem::create_directories(saveDir);
        SceneSerializer::Serialize(saveDir + "scene.json", rootObjects);
      }

      if (ImGui::MenuItem("Load Scene")) {
        SceneSerializer::Deserialize(saveDir + "scene.json", rootObjects);
        selectedObject_.reset(); // ロード後は選択をクリア
      }

      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void EditorUIManager::DrawToolbar() {
  ImGui::Begin("Toolbar");
  if (ImGui::RadioButton("Translate (Q)", currentGizmoOperation_ == ImGuizmo::TRANSLATE)) currentGizmoOperation_ = ImGuizmo::TRANSLATE;
  ImGui::SameLine();
  if (ImGui::RadioButton("Rotate (W)", currentGizmoOperation_ == ImGuizmo::ROTATE)) currentGizmoOperation_ = ImGuizmo::ROTATE;
  ImGui::SameLine();
  if (ImGui::RadioButton("Scale (R)", currentGizmoOperation_ == ImGuizmo::SCALE)) currentGizmoOperation_ = ImGuizmo::SCALE;
  
  ImGui::Separator();
  ImGui::Checkbox("Snap", &useSnap_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100.0f);
  ImGui::DragFloat("Snap Value", &snapValue_, 0.1f, 0.1f, 100.0f);
  
  ImGui::Separator();
  ImGui::Checkbox("Show Collider Debug", &showColliderDebug_);

  ImGui::End();
}

void EditorUIManager::DrawAssetBrowser(std::vector<std::shared_ptr<GameObject>>& rootObjects) {
  ImGui::Begin("Assets");
  std::string resourcesPath = AbsoluteEngine::EnginePath::Resolve("resources");
  
  if (std::filesystem::exists(resourcesPath)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(resourcesPath)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
        std::string payloadType = "";
        
        if (ext == ".obj" || ext == ".gltf") {
          color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); // Light blue
          payloadType = "ASSET_MODEL_PATH";
        } else if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
          color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // Green
          payloadType = "ASSET_TEXTURE_PATH";
        } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
          color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // Yellow
        } else if (ext == ".json") {
          color = ImVec4(1.0f, 0.6f, 0.8f, 1.0f); // Pink
          payloadType = "ASSET_PREFAB_PATH";
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        std::string filename = entry.path().filename().string();
        ImGui::Selectable(filename.c_str());
        
        if (!payloadType.empty() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
          std::string relPath = "resources/" + std::filesystem::relative(entry.path(), resourcesPath).string();
          std::replace(relPath.begin(), relPath.end(), '\\', '/');
          ImGui::SetDragDropPayload(payloadType.c_str(), relPath.c_str(), relPath.size() + 1);
          ImGui::Text("Drag %s", filename.c_str());
          ImGui::EndDragDropSource();
        }
        
        ImGui::PopStyleColor();
      }
    }
  }
  ImGui::End();
}

void EditorUIManager::DrawPrefabsBrowser(std::vector<std::shared_ptr<GameObject>>& rootObjects) {
  ImGui::Begin("Prefabs");
  std::string prefabsPath = AbsoluteEngine::EnginePath::Resolve("resources/prefabs");
  
  if (std::filesystem::exists(prefabsPath)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(prefabsPath)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        std::string filename = entry.path().filename().string();
        std::string prefabName = entry.path().stem().string();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.3f, 0.5f, 1.0f));
        
        if (ImGui::Button(prefabName.c_str(), ImVec2(-FLT_MIN, 30))) {
            // クリックでも配置できるようにする
            std::string fullPath = AbsoluteEngine::EnginePath::Resolve("resources/prefabs/" + filename);
            auto prefabInstance = SceneSerializer::LoadPrefab(fullPath);
            if (prefabInstance) {
                if (commandManager_) {
                  commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(prefabInstance, &rootObjects));
              } else {
                  rootObjects.push_back(prefabInstance);
                  SetSceneModified();
              }
                SelectObject(prefabInstance);
            }
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
          std::string relPath = "resources/prefabs/" + filename;
          ImGui::SetDragDropPayload("ASSET_PREFAB_PATH", relPath.c_str(), relPath.size() + 1);
          ImGui::Text("Place Prefab: %s", prefabName.c_str());
          ImGui::EndDragDropSource();
        }

        ImGui::PopStyleColor(3);
        ImGui::Spacing();
      }
    }
  } else {
    ImGui::Text("No prefabs found.");
  }
  ImGui::End();
}

void EditorUIManager::HandleShortcuts(std::vector<std::shared_ptr<GameObject>>& rootObjects, EditorCamera* camera) {
  // 右クリック押下中はカメラ操作のためショートカット無効
  if (ImGui::IsMouseDown(1)) return;

  ImGuiIO& io = ImGui::GetIO();
  if (!io.WantTextInput) { // テキスト入力中でない場合のみショートカットを有効化
    bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (commandManager_) commandManager_->Undo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (commandManager_) commandManager_->Redo();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Q)) currentGizmoOperation_ = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation_ = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation_ = ImGuizmo::SCALE;

    auto sel = selectedObject_.lock();
    if (sel) {
      if (ImGui::IsKeyPressed(ImGuiKey_F) && camera) {
        camera->FocusOn(sel->GetTransform().translate);
      }

      // Deleteキーで削除
      if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        auto parent = sel->GetParent();
        std::shared_ptr<ICommand> cmd;
        if (parent) {
          cmd = std::make_shared<DeleteObjectCommand>(sel, parent);
        } else {
          cmd = std::make_shared<DeleteObjectCommand>(sel, &rootObjects);
        }
        if (commandManager_) commandManager_->ExecuteCommand(cmd);
        selectedObject_.reset();
      }

      // Ctrl+Cでコピー
      if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        clipboardObject_ = SceneSerializer::CopyGameObject(sel);
      }
    }

    // Ctrl+Vでペースト
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V) && clipboardObject_) {
      auto newObj = SceneSerializer::CopyGameObject(clipboardObject_);
      std::shared_ptr<ICommand> cmd;
      if (sel) {
        cmd = std::make_shared<CreateObjectCommand>(newObj, sel);
      } else {
        cmd = std::make_shared<CreateObjectCommand>(newObj, &rootObjects);
      }
      if (commandManager_) commandManager_->ExecuteCommand(cmd);
      SelectObject(newObj); // ペーストしたものを選択状態に
    }
  }
}

void EditorUIManager::DrawViewport(std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
  // ImGuizmoの設定
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::BeginFrame();

  // "Viewport##GameView" ウィンドウ
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("Viewport##GameView");
  ImGui::PopStyleVar();

  ImVec2 viewportSize = ImGui::GetContentRegionAvail();
  if (viewportSize.x < 1.0f) viewportSize.x = 1.0f;
  if (viewportSize.y < 1.0f) viewportSize.y = 1.0f;

  auto* renderer = Renderer::GetInstance();
  D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = renderer->GetPostProcessTextureSrv();
  if (srvHandle.ptr != 0) {
      ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), viewportSize);
  }

  HandleViewportDragDrop(rootObjects, viewMatrix, projectionMatrix);

  // Viewportウィンドウ内でのマウスピッキング処理（BeginとEndの間で行うことでHoveredが正しくとれる）
  HandleMousePicking(rootObjects, viewMatrix, projectionMatrix);

  // タイトルバー等を除いた実際の描画領域を取得する
  ImVec2 vMin = ImGui::GetWindowContentRegionMin();
  ImVec2 vMax = ImGui::GetWindowContentRegionMax();
  ImVec2 wPos = ImGui::GetWindowPos();
  vMin.x += wPos.x;
  vMin.y += wPos.y;
  vMax.x += wPos.x;
  vMax.y += wPos.y;

  // デバッグ描画
  if (showColliderDebug_) {
    DrawColliderDebug(rootObjects, viewMatrix, projectionMatrix, vMin, vMax);
  }

  auto obj = selectedObject_.lock();
  if (!obj) {
    ImGui::End();
    return;
  }
  ImVec2 windowPos = ImGui::GetWindowPos();
  ImVec2 windowSize = ImGui::GetWindowSize();
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

  Transform& t = obj->GetTransform();

  // ImGuizmo用の変換（Translate, Rotate(度数法), Scale）
  float translation[3] = { t.translate.x, t.translate.y, t.translate.z };
  float rotation[3] = { t.rotate.x * 180.0f / 3.14159265f, t.rotate.y * 180.0f / 3.14159265f, t.rotate.z * 180.0f / 3.14159265f };
  float scale[3] = { t.scale.x, t.scale.y, t.scale.z };
  
  float objectMatrix[16];
  ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, objectMatrix);

  float snapValues[3] = { snapValue_, snapValue_, snapValue_ };

  // マニピュレーターの描画と操作
  ImGuizmo::Manipulate(&viewMatrix.m[0][0], &projectionMatrix.m[0][0], currentGizmoOperation_, ImGuizmo::LOCAL, objectMatrix, nullptr, useSnap_ ? snapValues : nullptr);

  // 操作されたら Transform に反映
  bool wasGizmoUsing = isGizmoUsing_;
  isGizmoUsing_ = ImGuizmo::IsUsing();

  if (!wasGizmoUsing && isGizmoUsing_) {
      // 操作開始時
      transformBeforeGizmo_ = obj->GetTransform();
  }

  if (isGizmoUsing_) {
    ImGuizmo::DecomposeMatrixToComponents(objectMatrix, translation, rotation, scale);
    t.translate = { translation[0], translation[1], translation[2] };
    t.rotate = { rotation[0] * 3.14159265f / 180.0f, rotation[1] * 3.14159265f / 180.0f, rotation[2] * 3.14159265f / 180.0f };
    t.scale = { scale[0], scale[1], scale[2] };
  }

  if (wasGizmoUsing && !isGizmoUsing_) {
      // 操作終了時
      auto afterTransform = obj->GetTransform();
      if (transformBeforeGizmo_.translate.x != afterTransform.translate.x || transformBeforeGizmo_.translate.y != afterTransform.translate.y || transformBeforeGizmo_.translate.z != afterTransform.translate.z ||
          transformBeforeGizmo_.rotate.x != afterTransform.rotate.x || transformBeforeGizmo_.rotate.y != afterTransform.rotate.y || transformBeforeGizmo_.rotate.z != afterTransform.rotate.z ||
          transformBeforeGizmo_.scale.x != afterTransform.scale.x || transformBeforeGizmo_.scale.y != afterTransform.scale.y || transformBeforeGizmo_.scale.z != afterTransform.scale.z) {
          
          auto cmd = std::make_shared<TransformCommand>(obj, transformBeforeGizmo_, afterTransform);
          if (commandManager_) {
              commandManager_->AddCommand(cmd);
          }
      }
  }

  ImGui::End();
}

void EditorUIManager::HandleMousePicking(const std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
  // Viewportウィンドウがホバーされているか、ギズモを操作中でないか確認
  if (!ImGui::IsWindowHovered() || ImGuizmo::IsOver()) return;

  // 左クリックされた瞬間のみ判定
  if (ImGui::IsMouseClicked(0)) {
    ImVec2 mousePos = ImGui::GetMousePos();
    
    // タイトルバー等を除いた実際の描画領域を取得する
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    ImVec2 wPos = ImGui::GetWindowPos();
    vMin.x += wPos.x;
    vMin.y += wPos.y;
    vMax.x += wPos.x;
    vMax.y += wPos.y;

    // 描画領域内での割合 (0.0 ~ 1.0)
    float u = (mousePos.x - vMin.x) / (vMax.x - vMin.x);
    float v = (mousePos.y - vMin.y) / (vMax.y - vMin.y);

    // 領域外をクリックした場合は何もしない（一応チェック）
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return;

    // NDC（正規化デバイス座標系: -1.0 ~ 1.0, Y軸反転）
    float ndcX = u * 2.0f - 1.0f;
    float ndcY = 1.0f - v * 2.0f;

    // ViewProjectionの逆行列を計算
    Matrix4x4 viewProj = Multiply(viewMatrix, projectionMatrix);
    Matrix4x4 invViewProj = Inverse(viewProj);

    // Nearクリップ面とFarクリップ面でのワールド座標を計算
    Vector3 nearPos = TransformPoint({ ndcX, ndcY, 0.0f }, invViewProj);
    Vector3 farPos = TransformPoint({ ndcX, ndcY, 1.0f }, invViewProj);

    // レイの方向ベクトル
    Vector3 rayDir = Normalize({ farPos.x - nearPos.x, farPos.y - nearPos.y, farPos.z - nearPos.z });

    // 一番手前で交差したオブジェクトを探す
    std::shared_ptr<GameObject> hitObject = nullptr;
    float minT = FLT_MAX; // 距離の最小値

    for (const auto& obj : rootObjects) {
      CheckIntersection(obj, nearPos, rayDir, hitObject, minT);
    }

    // 選択状態を更新
    if (hitObject) {
      SelectObject(hitObject);
    } else {
      selectedObject_.reset();
    }
  }
}

void EditorUIManager::CheckIntersection(std::shared_ptr<GameObject> obj, const Vector3& rayOrigin, const Vector3& rayDir, std::shared_ptr<GameObject>& hitObject, float& minT) {
  if (!obj) return;

  auto colliderComp = obj->GetComponent<ColliderComponent>();
  if (colliderComp && colliderComp->type != ColliderComponent::Type::None) {
    const auto& collider = *colliderComp;
    Vector3 objPos = obj->GetTransform().translate;
    Vector3 scale = obj->GetTransform().scale;
    Vector3 center = { objPos.x + collider.centerOffset.x * scale.x, 
                       objPos.y + collider.centerOffset.y * scale.y, 
                       objPos.z + collider.centerOffset.z * scale.z };

    if (collider.type == ColliderComponent::Type::Sphere) {
      float maxScale = (std::max)({ scale.x, scale.y, scale.z });
      float scaledRadius = collider.radius * maxScale;

      Vector3 m = { rayOrigin.x - center.x, rayOrigin.y - center.y, rayOrigin.z - center.z };
      float b = Dot(m, rayDir);
      float c = Dot(m, m) - scaledRadius * scaledRadius;

      float discriminant = b * b - c;
      if (discriminant > 0.0f) {
        float t = -b - std::sqrt(discriminant);
        if (t > 0.0f && t < minT) {
          minT = t;
          hitObject = obj;
        }
      }
    } else if (collider.type == ColliderComponent::Type::AABB) {
      Vector3 scaledSize = { collider.size.x * scale.x, collider.size.y * scale.y, collider.size.z * scale.z };
      Vector3 minBounds = { center.x - scaledSize.x, center.y - scaledSize.y, center.z - scaledSize.z };
      Vector3 maxBounds = { center.x + scaledSize.x, center.y + scaledSize.y, center.z + scaledSize.z };
      
      float t1 = (minBounds.x - rayOrigin.x) / (rayDir.x != 0.0f ? rayDir.x : 0.00001f);
      float t2 = (maxBounds.x - rayOrigin.x) / (rayDir.x != 0.0f ? rayDir.x : 0.00001f);
      float t3 = (minBounds.y - rayOrigin.y) / (rayDir.y != 0.0f ? rayDir.y : 0.00001f);
      float t4 = (maxBounds.y - rayOrigin.y) / (rayDir.y != 0.0f ? rayDir.y : 0.00001f);
      float t5 = (minBounds.z - rayOrigin.z) / (rayDir.z != 0.0f ? rayDir.z : 0.00001f);
      float t6 = (maxBounds.z - rayOrigin.z) / (rayDir.z != 0.0f ? rayDir.z : 0.00001f);
      
      float tmin = (std::max)((std::max)((std::min)(t1, t2), (std::min)(t3, t4)), (std::min)(t5, t6));
      float tmax = (std::min)((std::min)((std::max)(t1, t2), (std::max)(t3, t4)), (std::max)(t5, t6));
      
      if (tmax >= 0 && tmin <= tmax) {
        if (tmin < minT) {
          minT = tmin;
          hitObject = obj;
        }
      }
    }
  } else {
    // フォールバック: コライダーが無い場合でもモデルがあれば簡易的な球体判定を行う
    auto modelComp = obj->GetComponent<ModelComponent>();
    if (modelComp) {
      Vector3 objPos = obj->GetTransform().translate;
      Vector3 scale = obj->GetTransform().scale;
      float maxScale = (std::max)({ scale.x, scale.y, scale.z });
      float scaledRadius = 1.0f * maxScale; // デフォルト半径 1.0

      Vector3 m = { rayOrigin.x - objPos.x, rayOrigin.y - objPos.y, rayOrigin.z - objPos.z };
      float b = Dot(m, rayDir);
      float c = Dot(m, m) - scaledRadius * scaledRadius;

      float discriminant = b * b - c;
      if (discriminant > 0.0f) {
        float t = -b - std::sqrt(discriminant);
        if (t > 0.0f && t < minT) {
          minT = t;
          hitObject = obj;
        }
      }
    }
  }

  // 子ノードも再帰的にチェック
  for (const auto& child : obj->GetChildren()) {
    CheckIntersection(child, rayOrigin, rayDir, hitObject, minT);
  }
}

static bool WorldToScreen(const Vector3& worldPos, const Matrix4x4& viewProj, const ImVec2& vMin, const ImVec2& vMax, ImVec2& outScreenPos) {
  Vector3 clipCoords = TransformPoint(worldPos, viewProj);
  if (clipCoords.z < 0.0f || clipCoords.z > 1.0f) return false;

  float u = (clipCoords.x + 1.0f) * 0.5f;
  float v = (1.0f - clipCoords.y) * 0.5f;

  outScreenPos.x = vMin.x + u * (vMax.x - vMin.x);
  outScreenPos.y = vMin.y + v * (vMax.y - vMin.y);
  return true;
}

void EditorUIManager::DrawColliderDebug(const std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const ImVec2& vMin, const ImVec2& vMax) {
  Matrix4x4 viewProj = Multiply(viewMatrix, projectionMatrix);
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  ImU32 color = IM_COL32(0, 255, 0, 255); // 緑色

  auto drawObj = [&](auto& self, const std::shared_ptr<GameObject>& obj) -> void {
    if (!obj) return;

    auto colliderComp = obj->GetComponent<ColliderComponent>();
    if (colliderComp && colliderComp->type != ColliderComponent::Type::None) {
      const auto& collider = *colliderComp;
      Vector3 objPos = obj->GetTransform().translate;
      Vector3 scale = obj->GetTransform().scale;
      Vector3 center = { objPos.x + collider.centerOffset.x * scale.x, 
                         objPos.y + collider.centerOffset.y * scale.y, 
                         objPos.z + collider.centerOffset.z * scale.z };

      if (collider.type == ColliderComponent::Type::AABB) {
        Vector3 scaledSize = { collider.size.x * scale.x, collider.size.y * scale.y, collider.size.z * scale.z };
        
        Vector3 corners[8] = {
          {center.x - scaledSize.x, center.y - scaledSize.y, center.z - scaledSize.z},
          {center.x + scaledSize.x, center.y - scaledSize.y, center.z - scaledSize.z},
          {center.x + scaledSize.x, center.y + scaledSize.y, center.z - scaledSize.z},
          {center.x - scaledSize.x, center.y + scaledSize.y, center.z - scaledSize.z},
          {center.x - scaledSize.x, center.y - scaledSize.y, center.z + scaledSize.z},
          {center.x + scaledSize.x, center.y - scaledSize.y, center.z + scaledSize.z},
          {center.x + scaledSize.x, center.y + scaledSize.y, center.z + scaledSize.z},
          {center.x - scaledSize.x, center.y + scaledSize.y, center.z + scaledSize.z}
        };

        ImVec2 screenCorners[8];
        bool visible[8];
        for (int i = 0; i < 8; ++i) {
          visible[i] = WorldToScreen(corners[i], viewProj, vMin, vMax, screenCorners[i]);
        }

        auto drawLine = [&](int i, int j) {
          if (visible[i] && visible[j]) {
            drawList->AddLine(screenCorners[i], screenCorners[j], color);
          }
        };

        drawLine(0, 1); drawLine(1, 2); drawLine(2, 3); drawLine(3, 0);
        drawLine(4, 5); drawLine(5, 6); drawLine(6, 7); drawLine(7, 4);
        drawLine(0, 4); drawLine(1, 5); drawLine(2, 6); drawLine(3, 7);

      } else if (collider.type == ColliderComponent::Type::Sphere) {
        float maxScale = (std::max)({ scale.x, scale.y, scale.z });
        float r = collider.radius * maxScale;
        
        Vector3 points[6] = {
          {center.x - r, center.y, center.z}, {center.x + r, center.y, center.z},
          {center.x, center.y - r, center.z}, {center.x, center.y + r, center.z},
          {center.x, center.y, center.z - r}, {center.x, center.y, center.z + r}
        };
        ImVec2 sp[6];
        bool v[6];
        for (int i = 0; i < 6; ++i) v[i] = WorldToScreen(points[i], viewProj, vMin, vMax, sp[i]);

        if(v[0]&&v[1]) drawList->AddLine(sp[0], sp[1], color);
        if(v[2]&&v[3]) drawList->AddLine(sp[2], sp[3], color);
        if(v[4]&&v[5]) drawList->AddLine(sp[4], sp[5], color);
      }
    }

    for (const auto& child : obj->GetChildren()) {
      self(self, child);
    }
  };

  for (const auto& root : rootObjects) {
    drawObj(drawObj, root);
  }
}
#endif
#ifdef USE_IMGUI
void EditorUIManager::DrawHierarchy(std::vector<std::shared_ptr<GameObject>>& rootObjects) {
  ImGui::Begin("Hierarchy");

  // イテレータ無効化を防ぐため、ループ前にスナップショットを取ってから描画する
  // （ドラッグ＆ドロップやコンテキストメニュー等でループ中に rootObjects が変更されても安全）
  auto rootSnapshot = rootObjects;
  for (const auto& obj : rootSnapshot) {
    DrawGameObjectNode(obj, rootObjects);
  }

  // Hierarchyウィンドウ全体へのドロップ受け入れ
  ImGui::Dummy(ImGui::GetContentRegionAvail()); // 残りのスペースをダミーで埋める
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PREFAB_PATH")) {
      const char* payloadPath = (const char*)payload->Data;
      std::string fullPath = AbsoluteEngine::EnginePath::Resolve(std::string(payloadPath));
      auto prefabInstance = SceneSerializer::LoadPrefab(fullPath);
      if (prefabInstance) {
        if (commandManager_) {
          commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(prefabInstance, &rootObjects));
        } else {
          rootObjects.push_back(prefabInstance);
          SetSceneModified();
        }
        SelectObject(prefabInstance);
      }
    }
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MODEL_PATH")) {
      const char* payloadPath = (const char*)payload->Data;
      std::filesystem::path p(payloadPath);
      std::string objName = p.stem().string();
      if (objName.empty()) objName = "Model";

      auto newObj = std::make_shared<AbsoluteEngine::GameObject>(objName);
      auto modelComp = std::make_unique<ModelComponent>();
      modelComp->LoadModel(payloadPath);
      newObj->AddComponent(std::move(modelComp));
      newObj->GetTransform().translate = {0, 0, 0};
      
      if (commandManager_) {
        commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(newObj, &rootObjects));
      } else {
        rootObjects.push_back(newObj);
        SetSceneModified();
      }
      SelectObject(newObj);
    }
    ImGui::EndDragDropTarget();
  }

  // 余白をクリックしたら選択解除
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
    selectedObject_.reset();
  }

  // 右クリックメニュー（新規作成）
  if (ImGui::BeginPopupContextWindow("HierarchyContextWindow", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      if (ImGui::Selectable("Create Empty")) {
          auto newObj = std::make_shared<GameObject>("GameObject");
          if (commandManager_) {
              commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(newObj, &rootObjects));
          } else {
              rootObjects.push_back(newObj);
              SetSceneModified();
          }
          SelectObject(newObj);
      }
      if (ImGui::BeginMenu("Create Light")) {
          if (ImGui::Selectable("Directional Light")) {
              auto newObj = std::make_shared<GameObject>("Directional Light");
              auto lightComp = std::make_unique<LightNodeComponent>();
              lightComp->type = LightNodeComponent::Type::Directional;
              newObj->AddComponent(std::move(lightComp));
              newObj->GetTransform().rotate = { 0.5f, 0.5f, 0.0f };
              if (commandManager_) { commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(newObj, &rootObjects)); } else { rootObjects.push_back(newObj); SetSceneModified(); }
              SelectObject(newObj);
          }
          if (ImGui::Selectable("Point Light")) {
              auto newObj = std::make_shared<GameObject>("Point Light");
              auto lightComp = std::make_unique<LightNodeComponent>();
              lightComp->type = LightNodeComponent::Type::Point;
              newObj->AddComponent(std::move(lightComp));
              if (commandManager_) { commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(newObj, &rootObjects)); } else { rootObjects.push_back(newObj); SetSceneModified(); }
              SelectObject(newObj);
          }
          if (ImGui::Selectable("Spot Light")) {
              auto newObj = std::make_shared<GameObject>("Spot Light");
              auto lightComp = std::make_unique<LightNodeComponent>();
              lightComp->type = LightNodeComponent::Type::Spot;
              newObj->AddComponent(std::move(lightComp));
              if (commandManager_) { commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(newObj, &rootObjects)); } else { rootObjects.push_back(newObj); SetSceneModified(); }
              SelectObject(newObj);
          }
          ImGui::EndMenu();
      }
      ImGui::EndPopup();
  }

  ImGui::End();
}

void EditorUIManager::SelectObject(const std::shared_ptr<GameObject>& obj) {
    selectedObject_ = obj;
    // オブジェクトを選び直したら、タイムラインイベント側の選択状態は解除する。
    // DrawInspector()はイベント選択があると常にそちらを優先表示するため、
    // ここでクリアしないとHierarchy/Viewportでの選び直しがインスペクタに反映されない。
    if (timelineManager_) {
        timelineManager_->SetSelectedEvent(nullptr);
    }
}

void EditorUIManager::DrawGameObjectNode(std::shared_ptr<GameObject> obj, std::vector<std::shared_ptr<GameObject>>& rootObjects) {
  if (!obj) return;

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
  
  // 現在選択されているノードならハイライト
  if (selectedObject_.lock() == obj) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  
  // 子がいない場合は葉ノードとして表示
  if (obj->GetChildren().empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  bool isOpen = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s", obj->GetName().c_str());

  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    SelectObject(obj);
  }

  if (isOpen && !obj->GetChildren().empty()) {
    for (const auto& child : obj->GetChildren()) {
      DrawGameObjectNode(child, rootObjects);
    }
    ImGui::TreePop();
  }
}

void EditorUIManager::DrawInspector() {
  ImGui::Begin("Inspector");

  // タイムラインイベントが選択されている場合はイベント専用UIを描画して早期リターン（タスク15）
  if (timelineManager_ && timelineManager_->GetSelectedEvent() != nullptr) {
      DrawTimelineEventInspector();
      ImGui::End();
      return;
  }

  auto obj = selectedObject_.lock();
  if (obj) {
    // 名前の編集
    char nameBuffer[256];
    strncpy_s(nameBuffer, obj->GetName().c_str(), sizeof(nameBuffer));
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
      obj->SetName(nameBuffer);
      SetSceneModified();
    }

    // タグの編集
    char tagBuffer[256];
    strncpy_s(tagBuffer, obj->GetTag().c_str(), sizeof(tagBuffer));
    if (ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer))) {
      obj->SetTag(tagBuffer);
      SetSceneModified();
    }

    ImGui::Separator();
    
    // モデルとテクスチャの設定表示
    if (ImGui::CollapsingHeader("Model & Texture", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto modelComp = obj->GetComponent<ModelComponent>();
      if (modelComp) {
        ImGui::Text("Model: %s", modelComp->GetModelPath().empty() ? "None" : modelComp->GetModelPath().c_str());
        ImGui::Text("Texture: %s", modelComp->GetTexturePath().empty() ? "None" : modelComp->GetTexturePath().c_str());
        
        // テクスチャ適用用のドロップエリア
        ImGui::Button("Drop Texture Here (.png/.dds)", ImVec2(-FLT_MIN, 30));
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE_PATH")) {
            const char* payloadPath = (const char*)payload->Data;
            modelComp->LoadTexture(payloadPath);
            SetSceneModified();
          }
          ImGui::EndDragDropTarget();
        }

        // モデル適用用のドロップエリア
        ImGui::Button("Drop Model Here (.obj)", ImVec2(-FLT_MIN, 30));
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MODEL_PATH")) {
            const char* payloadPath = (const char*)payload->Data;
            modelComp->LoadModel(payloadPath);
            SetSceneModified();
          }
          ImGui::EndDragDropTarget();
        }
      } else {
        ImGui::Text("No ModelComponent attached.");
        if (ImGui::Button("Add ModelComponent")) {
          obj->AddComponent(std::make_unique<ModelComponent>());
          SetSceneModified();
        }
      }
    }

    ImGui::Separator();

    // Transformの編集
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      Transform& transform = obj->GetTransform();

      // Vector3の配列としてImGuiに渡すためのテンポラリ変数
      float t[3] = { transform.translate.x, transform.translate.y, transform.translate.z };
      float r[3] = { transform.rotate.x, transform.rotate.y, transform.rotate.z };
      float s[3] = { transform.scale.x, transform.scale.y, transform.scale.z };

      bool activated = false;
      bool deactivatedAfterEdit = false;

      if (ImGui::DragFloat3("Position", t, 0.1f)) {
        transform.translate = { t[0], t[1], t[2] };
      }
      activated |= ImGui::IsItemActivated();
      deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

      if (ImGui::DragFloat3("Rotation", r, 0.01f)) {
        transform.rotate = { r[0], r[1], r[2] };
      }
      activated |= ImGui::IsItemActivated();
      deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

      if (ImGui::DragFloat3("Scale", s, 0.1f)) {
        transform.scale = { s[0], s[1], s[2] };
      }
      activated |= ImGui::IsItemActivated();
      deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

      if (activated) {
          // 入力開始時のTransformを保存（DragFloat開始前）
          // t, r, s の値は画面上の値なので、実際のtransformの値を保持する
          // ※DragFloatの同じフレームで呼ばれるとすでに変わっている可能性があるので注意
          transformBeforeInspector_.translate = { t[0], t[1], t[2] };
          transformBeforeInspector_.rotate = { r[0], r[1], r[2] };
          transformBeforeInspector_.scale = { s[0], s[1], s[2] };
      }

      if (deactivatedAfterEdit) {
          if (commandManager_) {
              commandManager_->AddCommand(std::make_shared<TransformCommand>(obj, transformBeforeInspector_, transform));
          }
      }
    }

    // --- LightComponent ---
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto lightComp = obj->GetComponent<LightNodeComponent>();
      if (lightComp) {
        LightNodeComponent& l = *lightComp;
        int currentType = static_cast<int>(l.type);
        const char* lightTypes[] = { "None", "Directional", "Point", "Spot" };

        bool activated = false;
        bool deactivatedAfterEdit = false;

        if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
          l.type = static_cast<LightNodeComponent::Type>(currentType);
        }
        activated |= ImGui::IsItemActivated();
        deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

        if (l.type != LightNodeComponent::Type::None) {
          float color[3] = { l.color.x, l.color.y, l.color.z };
          if (ImGui::ColorEdit3("Color", color)) {
            l.color = { color[0], color[1], color[2] };
          }
          activated |= ImGui::IsItemActivated();
          deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

          ImGui::DragFloat("Intensity", &l.intensity, 0.05f, 0.0f, 100.0f);
          activated |= ImGui::IsItemActivated();
          deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
          
          if (l.type == LightNodeComponent::Type::Point || l.type == LightNodeComponent::Type::Spot) {
            ImGui::DragFloat("Radius", &l.radius, 0.1f, 0.0f, 1000.0f);
            activated |= ImGui::IsItemActivated();
            deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

            ImGui::DragFloat("Decay", &l.decay, 0.05f, 0.0f, 10.0f);
            activated |= ImGui::IsItemActivated();
            deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
          }
          if (l.type == LightNodeComponent::Type::Spot) {
            ImGui::DragFloat("Distance", &l.distance, 0.1f, 0.0f, 1000.0f);
            activated |= ImGui::IsItemActivated();
            deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

            ImGui::DragFloat("ConeAngle (deg)", &l.coneAngleDeg, 0.5f, 0.0f, 180.0f);
            activated |= ImGui::IsItemActivated();
            deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
          }
        }

        if (activated) {
            obj->GetComponent<LightNodeComponent>()->Serialize(componentStateBefore_);
        }
        if (deactivatedAfterEdit) {
            if (commandManager_) {
                nlohmann::json componentStateAfter;
                l.Serialize(componentStateAfter);
                commandManager_->AddCommand(std::make_shared<ComponentStateCommand>(obj, "LightNodeComponent", componentStateBefore_, componentStateAfter));
            }
        }
      } else {
        ImGui::Text("No LightNodeComponent attached.");
        if (ImGui::Button("Add LightNodeComponent")) {
          obj->AddComponent(std::make_unique<LightNodeComponent>());
          SetSceneModified();
        }
      }
    }

    // --- Dissolve ---
    if (ImGui::CollapsingHeader("Dissolve", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto dissolveComp = obj->GetComponent<DissolveComponent>();
      if (dissolveComp) {
        auto& dissolve = *dissolveComp;
        if (ImGui::Checkbox("Enable Dissolve", &dissolve.enable)) SetSceneModified();
        if (dissolve.enable) {
          if (ImGui::SliderFloat("Threshold", &dissolve.threshold, 0.0f, 1.0f)) SetSceneModified();
          if (ImGui::SliderFloat("Edge Range", &dissolve.edgeRange, 0.0f, 0.1f)) SetSceneModified();
          float edgeCol[3] = { dissolve.edgeColor.x, dissolve.edgeColor.y, dissolve.edgeColor.z };
          if (ImGui::ColorEdit3("Edge Color", edgeCol)) {
            dissolve.edgeColor = { edgeCol[0], edgeCol[1], edgeCol[2] };
            SetSceneModified();
          }
          float maskCol[3] = { dissolve.maskColor.x, dissolve.maskColor.y, dissolve.maskColor.z };
          if (ImGui::ColorEdit3("Mask Color", maskCol)) {
            dissolve.maskColor = { maskCol[0], maskCol[1], maskCol[2] };
            SetSceneModified();
          }
        }
      } else {
        ImGui::Text("No DissolveComponent attached.");
        if (ImGui::Button("Add DissolveComponent")) {
          obj->AddComponent(std::make_unique<DissolveComponent>());
          SetSceneModified();
        }
      }
    }

    // Colliderの編集
    if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto colliderComp = obj->GetComponent<ColliderComponent>();
      if (colliderComp) {
        ColliderComponent& collider = *colliderComp;

        const char* types[] = { "None", "Sphere", "AABB" };
        int currentType = static_cast<int>(collider.type);
        if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
          collider.type = static_cast<ColliderComponent::Type>(currentType);
          SetSceneModified();
        }

        if (collider.type != ColliderComponent::Type::None) {
          if (ImGui::DragFloat3("Center Offset", &collider.centerOffset.x, 0.1f)) SetSceneModified();

          if (collider.type == ColliderComponent::Type::Sphere) {
            if (ImGui::DragFloat("Radius", &collider.radius, 0.1f, 0.0f)) SetSceneModified();
          } else if (collider.type == ColliderComponent::Type::AABB) {
            if (ImGui::DragFloat3("Size (Half Extents)", &collider.size.x, 0.1f, 0.0f)) SetSceneModified();
          }
        }
      } else {
        ImGui::Text("No ColliderComponent attached.");
        if (ImGui::Button("Add ColliderComponent")) {
          obj->AddComponent(std::make_unique<ColliderComponent>());
          SetSceneModified();
        }
      }
    }

    ImGui::Separator();
    
    // プレハブ関連操作
    if (ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (obj->IsPrefabInstance()) {
        ImGui::Text("Prefab: %s", obj->GetPrefabPath().c_str());
      } else {
        ImGui::Text("Prefab: Not a prefab instance.");
      }

      if (ImGui::Button("Save as Prefab", ImVec2(-FLT_MIN, 30))) {
        std::string prefabDir = AbsoluteEngine::EnginePath::Resolve("resources/prefabs/");
        std::filesystem::create_directories(prefabDir);
        std::string filename = obj->GetName() + ".json";
        std::string filepath = prefabDir + filename;
        if (SceneSerializer::SavePrefab(filepath, obj)) {
            obj->SetPrefabPath(filepath);
            // 保存直後にPrefabRegistryを再スキャンし、Timeline Editor側のプレハブ選択肢に即反映する
            AbsoluteEngine::PrefabRegistry::GetInstance().ScanDirectory(prefabDir);
        }
      }
    }

    ImGui::Separator();

    // コンポーネント管理
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
      IComponent* componentToRemove = nullptr;
      
      for (const auto& comp : obj->GetComponents()) {
        ImGui::Text("- %s", comp->GetTypeName().c_str());
        
        // 追加：各コンポーネントのインスペクタ用UIを描画
        comp->DrawInspectorUI();

        ImGui::SameLine();
        
        // ボタンIDを一意にするためにポインタアドレスを使用
        std::string btnLabel = "Remove##" + std::to_string(reinterpret_cast<uintptr_t>(comp.get()));
        if (ImGui::Button(btnLabel.c_str())) {
            componentToRemove = comp.get();
            SetSceneModified();
        }
        // 今後はここで comp->DrawInspector() などを呼んでパラメータ編集できるようにする
      }

      if (componentToRemove) {
          obj->RemoveComponent(componentToRemove);
      }

      ImGui::Spacing();
      if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
      }

      if (ImGui::BeginPopup("AddComponentPopup")) {
        auto names = ComponentFactory::GetInstance().GetRegisteredComponentNames();
        if (names.empty()) {
          ImGui::TextDisabled("No components registered.");
        } else {
          for (const auto& name : names) {
            if (ImGui::Selectable(name.c_str())) {
              auto comp = ComponentFactory::GetInstance().Create(name);
              if (comp) {
                obj->AddComponent(std::move(comp));
                SetSceneModified();
              }
            }
          }
        }
        ImGui::EndPopup();
      }
    }

  } else {
    ImGui::Text("No Object Selected");
  }

  ImGui::End();
}

// ============================================================
// タスク15: タイムラインイベント選択時の専用インスペクタUI
// ============================================================
void EditorUIManager::DrawTimelineEventInspector() {
    if (!timelineManager_) return;

    ITimelineEvent* selectedEvent = timelineManager_->GetSelectedEvent();
    if (!selectedEvent) return;

    ImGui::TextColored({ 1.0f, 0.85f, 0.2f, 1.0f }, "[ Timeline Event Inspector ]");
    ImGui::Separator();

    // 編集開始/終了を横断して集計する（複数ウィジェットのいずれかが該当すればtrue）
    bool activated = false;
    bool deactivatedAfterEdit = false;

    // --- 発火時刻 (Time) ---
    // 発火時刻はトラックが管理している。どのトラックにも無い（通常は起きない）場合は欄を出さない
    float triggerTime = 0.0f;
    if (timelineManager_->GetEventTriggerTime(selectedEvent, triggerTime)) {
        ImGui::Text("Time: %.3f s", triggerTime);
        ImGui::SameLine();
        // DragFloatで直接編集可能にする。変更はトラック経由で行い、イベントの並び順をトラックが保つ
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::DragFloat("##TriggerTime", &triggerTime, 0.01f, 0.0f, timelineManager_->GetDuration(), "%.3f s")) {
            timelineManager_->SetEventTriggerTime(selectedEvent, triggerTime);
        }
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::Separator();

    // --- イベント種別ごとの固有プロパティ ---
    // dynamic_castでの型分岐はせず、ITimelineEvent::DrawInspectorUI()をポリモーフィックに
    // 呼び出す（IComponent::DrawInspectorUI()と同じ考え方）。これにより、エディタ側は
    // 具象イベント型（Application層に置かれるものも含む）を一切知らなくてよい。
    ImGui::Text("Type: %s", selectedEvent->GetEventType().c_str());
    ImGui::Separator();
    selectedEvent->DrawInspectorUI(activated, deactivatedAfterEdit);

    // --- Undo/Redo用スナップショットの取得・コマンド発行（タスクF） ---
    // 毎フレームスタックが積まれるのを防ぐため、編集開始時(Activated)にスナップショットを取り、
    // 編集完了時(DeactivatedAfterEdit)にのみコマンドを発行する
    if (activated) {
        timelineSnapshotBeforeEdit_ = timelineManager_->SerializeToString();
    }
    if (deactivatedAfterEdit) {
        const std::string beforeSnapshot = timelineSnapshotBeforeEdit_;
        const std::string afterSnapshot = timelineManager_->SerializeToString();
        if (beforeSnapshot != afterSnapshot) {
            if (commandManager_) {
                auto* mgr = timelineManager_;
                auto cmd = std::make_shared<TimelineCommand>(
                    [mgr, beforeSnapshot]() { mgr->LoadFromString(beforeSnapshot); },
                    [mgr, afterSnapshot]() { mgr->LoadFromString(afterSnapshot); }
                );
                commandManager_->AddCommand(cmd);
            }
            timelineManager_->SetDirty(true);
            SetSceneModified();
        }
    }

    // --- 選択解除ボタン ---
    if (ImGui::Button("Deselect Event")) {
        timelineManager_->SetSelectedEvent(nullptr);
    }
}
#endif


void EditorUIManager::DrawCameraControls(EditorCamera& camera) {
#ifdef USE_IMGUI
  // 入力欄が操作されたときだけカメラへ書き戻す
  Vector3 position = camera.GetTranslate();
  if (ImGui::DragFloat3("Camera Pos", &position.x, 0.1f)) {
    camera.SetTranslate(position);
  }
  Vector3 rotation = camera.GetRotation();
  if (ImGui::DragFloat3("Camera Rot(Pitch,Yaw,Roll)", &rotation.x, 0.05f)) {
    camera.SetRotation(rotation);
  }
#else
  (void)camera;
#endif
}

void EditorUIManager::HandleViewportDragDrop(std::vector<std::shared_ptr<GameObject>>& rootObjects, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
#ifdef USE_IMGUI
  if (ImGui::BeginDragDropTarget()) {
    
    // 配置先のワールド座標を計算
    Vector3 targetPos = {0, 0, 0};
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    ImVec2 wPos = ImGui::GetWindowPos();
    vMin.x += wPos.x;
    vMin.y += wPos.y;
    vMax.x += wPos.x;
    vMax.y += wPos.y;

    float u = (mousePos.x - vMin.x) / (vMax.x - vMin.x);
    float v = (mousePos.y - vMin.y) / (vMax.y - vMin.y);

    if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            dropDistance_ += wheel * 2.0f; // ホイール1段階につき2m移動
            if (dropDistance_ < 1.0f) dropDistance_ = 1.0f; // 最小値
        }

        float ndcX = u * 2.0f - 1.0f;
        float ndcY = 1.0f - v * 2.0f;

        Matrix4x4 viewProj = Multiply(viewMatrix, projectionMatrix);
        Matrix4x4 invViewProj = Inverse(viewProj);

        Vector3 nearPos = TransformPoint({ ndcX, ndcY, 0.0f }, invViewProj);
        Vector3 farPos = TransformPoint({ ndcX, ndcY, 1.0f }, invViewProj);
        Vector3 rayDir = Normalize({ farPos.x - nearPos.x, farPos.y - nearPos.y, farPos.z - nearPos.z });

        // カメラから一定距離(dropDistance_)の座標をドロップ先とする
        targetPos = { nearPos.x + rayDir.x * dropDistance_, nearPos.y + rayDir.y * dropDistance_, nearPos.z + rayDir.z * dropDistance_ };

        // プレビュー表示: マーカーを描画
        ImVec2 screenPos;
        if (WorldToScreen(targetPos, viewProj, vMin, vMax, screenPos)) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            // 距離を表示するテキストも追加すると親切
            char distText[32];
            snprintf(distText, sizeof(distText), "%.1fm", dropDistance_);
            drawList->AddText(ImVec2(screenPos.x + 10, screenPos.y + 10), IM_COL32(255, 255, 0, 255), distText);
            
            drawList->AddCircle(screenPos, 5.0f, IM_COL32(255, 255, 0, 255), 12, 2.0f);
            drawList->AddLine(ImVec2(screenPos.x - 10, screenPos.y), ImVec2(screenPos.x + 10, screenPos.y), IM_COL32(255, 255, 0, 255), 2.0f);
            drawList->AddLine(ImVec2(screenPos.x, screenPos.y - 10), ImVec2(screenPos.x, screenPos.y + 10), IM_COL32(255, 255, 0, 255), 2.0f);
        }
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PREFAB_PATH", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
      if (payload->IsDelivery()) {
        const char* payloadPath = (const char*)payload->Data;
        std::string fullPath = AbsoluteEngine::EnginePath::Resolve(std::string(payloadPath));
        auto prefabInstance = SceneSerializer::LoadPrefab(fullPath);
        if (prefabInstance) {
          prefabInstance->GetTransform().translate = targetPos;
          if (commandManager_) {
            commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(prefabInstance, &rootObjects));
          } else {
            rootObjects.push_back(prefabInstance);
          }
          SelectObject(prefabInstance);
        }
      }
    }
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MODEL_PATH", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
      if (payload->IsDelivery()) {
        const char* payloadPath = (const char*)payload->Data;
        std::filesystem::path p(payloadPath);
        std::string objName = p.stem().string();
        if (objName.empty()) objName = "Model";

        auto newObj = std::make_shared<AbsoluteEngine::GameObject>(objName);
        auto modelComp = std::make_unique<ModelComponent>();
        modelComp->LoadModel(payloadPath);
        newObj->AddComponent(std::move(modelComp));
        newObj->GetTransform().translate = targetPos;
        
        if (commandManager_) {
          commandManager_->ExecuteCommand(std::make_shared<CreateObjectCommand>(newObj, &rootObjects));
        } else {
          rootObjects.push_back(newObj);
        }
        SelectObject(newObj);
      }
    }
    ImGui::EndDragDropTarget();
  }
#endif
}

#ifdef USE_IMGUI
void EditorUIManager::DrawPostProcessSettings() {
    ImGui::Begin("Post Process Settings");
    
    if (ImGui::RadioButton("Normal", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::Normal))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::Normal);
    ImGui::SameLine();
    if (ImGui::RadioButton("Grayscale", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::Grayscale))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::Grayscale);
    ImGui::SameLine();
    if (ImGui::RadioButton("Sepia", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::Sepia))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::Sepia);
    ImGui::SameLine();
    if (ImGui::RadioButton("Vignette", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::Vignette))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::Vignette);
    ImGui::SameLine();
    if (ImGui::RadioButton("BoxFilter", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::BoxFilter))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::BoxFilter);
    ImGui::SameLine();
    if (ImGui::RadioButton("GaussianFilter", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::GaussianFilter))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::GaussianFilter);
    ImGui::SameLine();
    if (ImGui::RadioButton("LuminanceOutline", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::LuminanceBasedOutline))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::LuminanceBasedOutline);
    ImGui::SameLine();
    if (ImGui::RadioButton("DepthOutline", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::DepthBasedOutline))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::DepthBasedOutline);
    ImGui::SameLine();
    if (ImGui::RadioButton("RadialBlur", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::RadialBlur))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::RadialBlur);
    ImGui::SameLine();
    if (ImGui::RadioButton("Dissolve", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::Dissolve))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::Dissolve);
    ImGui::SameLine();
    if (ImGui::RadioButton("Random", &postProcessMode_, static_cast<int>(Renderer::PostProcessMode::Random))) postProcessMode_ = static_cast<int>(Renderer::PostProcessMode::Random);

    auto* renderer = Renderer::GetInstance();
    renderer->SetPostProcessMode(static_cast<Renderer::PostProcessMode>(postProcessMode_));
    if (postProcessMode_ == static_cast<int>(Renderer::PostProcessMode::Vignette)) {
        ImGui::SliderFloat("Vignette Scale", &vignetteScale_, 1.0f, 32.0f);
        ImGui::SliderFloat("Vignette Pow", &vignettePow_, 0.1f, 5.0f);
        renderer->SetVignetteParam(vignetteScale_, vignettePow_);
    } else if (postProcessMode_ == static_cast<int>(Renderer::PostProcessMode::BoxFilter)) {
        ImGui::SliderInt("BoxFilter K", &boxFilterK_, 1, 10);
        renderer->SetBoxFilterParam(boxFilterK_);
    } else if (postProcessMode_ == static_cast<int>(Renderer::PostProcessMode::GaussianFilter)) {
        ImGui::SliderInt("GaussianFilter K", &gaussianFilterK_, 1, 10);
        ImGui::SliderFloat("GaussianFilter Sigma", &gaussianFilterSigma_, 0.1f, 10.0f);
        renderer->SetGaussianFilterParam(gaussianFilterK_, gaussianFilterSigma_, {1, 1});
    } else if (postProcessMode_ == static_cast<int>(Renderer::PostProcessMode::Dissolve)) {
        ImGui::SliderFloat("Threshold", &dissolveThreshold_, 0.0f, 1.0f);
        ImGui::ColorEdit3("Edge Color", &dissolveEdgeColor_.x);
        ImGui::SliderFloat("Edge Range", &dissolveEdgeRange_, 0.01f, 0.5f);
        renderer->SetDissolveParam(dissolveThreshold_, dissolveEdgeRange_, dissolveEdgeColor_, {1, 1, 1});
    }
    
    ImGui::End();
}

#endif

} // namespace AbsoluteEngine
