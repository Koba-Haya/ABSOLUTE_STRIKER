#define NOMINMAX
#include "RailCameraComponent.h"
#include "GameCamera.h"
#include "Method.h"
#include "Renderer.h"
#include "Spline.h"
#include "../../AbsoluteEngine/scene/BaseScene.h"
#include "../actor/Player/PlayerComponent.h"
#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#include "../../externals/ImGuizmo/ImGuizmo.h"
#include "../../AbsoluteEngine/scene/BaseScene.h"
#include "../../AbsoluteEngine/editor/CommandManager.h"
#include "../../AbsoluteEngine/editor/Command.h"
#endif

RailCameraComponent::RailCameraComponent() {
    waypoints_ = {
        { 0.0f,  5.0f, -50.0f}, // P0: スタート（少し手前から）
        { 0.0f,  3.0f,   0.0f}, // P1: 序盤の直進エリア
        { 20.0f, 8.0f,  70.0f}, // P2: 大きく右へ上昇カーブ
        {-15.0f, 2.0f, 140.0f}, // P3: 左下へと急降下旋回
        { 0.0f,  5.0f, 220.0f}  // P4: ゴールへと向かう直進路
    };
    speed_ = 15.0f; // 秒速15ユニットで走破する（ロックオンの余裕を持たせた速度）
    lookAheadOffset_ = 0.02f;
    RebuildArcLengthTable_();
}

void RailCameraComponent::DrawDebugLine(Renderer* renderer) const {
    if (waypoints_.size() < 2) return;

    Matrix4x4 viewMat = renderer->GetViewMatrix();
    Matrix4x4 invView = Inverse(viewMat);
    Vector3 camEye = { invView.m[3][0], invView.m[3][1], invView.m[3][2] };
    Vector3 camForward = { invView.m[2][0], invView.m[2][1], invView.m[2][2] };

    auto isInFront = [&](const Vector3& p) {
        Vector3 diff = { p.x - camEye.x, p.y - camEye.y, p.z - camEye.z };
        return Dot(diff, camForward) > 0.0f;
    };

    Vector4 color = {1.0f, 1.0f, 0.0f, 1.0f}; // 黄色
    const int subdivisions = 100;
    Vector3 prevPoint = Spline::GetPoint(waypoints_, 0.0f);
    for (int i = 1; i <= subdivisions; ++i) {
        float t = (float)i / (float)subdivisions;
        Vector3 currPoint = Spline::GetPoint(waypoints_, t);
        if (isInFront(prevPoint) && isInFront(currPoint)) {
            renderer->DrawLine(prevPoint, currPoint, color);
        }
        prevPoint = currPoint;
    }
    for (const auto& wp : waypoints_) {
        if (isInFront(wp)) {
            renderer->DrawLine(Vector3{wp.x, wp.y - 1.0f, wp.z}, Vector3{wp.x, wp.y + 1.0f, wp.z}, Vector4{1, 0, 0, 1});
            renderer->DrawLine(Vector3{wp.x - 1.0f, wp.y, wp.z}, Vector3{wp.x + 1.0f, wp.y, wp.z}, Vector4{1, 0, 0, 1});
        }
    }
}

void RailCameraComponent::Update(float deltaTime) {
    if (waypoints_.size() < 2) return;
    auto* scene = BaseScene::GetActiveScene();
    if (!scene) return;
    auto* camera = scene->GetMainCamera();
    if (!camera) return;

    // 進捗の更新（弧長ベース：秒速speed_ユニットで進むぶんを全体距離に対する割合に変換する）
    const float totalLength = arcLengthTable_.GetTotalLength();
    if (totalLength > 0.0f) {
        progress_ += (speed_ * deltaTime) / totalLength;
    }
    if (progress_ > 1.0f) progress_ = 1.0f;

    // カメラ位置を計算して反映する（SetProgressと共通処理）
    ApplyCameraTransform_();
}

void RailCameraComponent::SetProgress(float progress) {
    progress_ = std::clamp(progress, 0.0f, 1.0f);

    // progress_ を書き換えた直後にカメラ位置を即座に再計算する
    // これにより、エディットモードでシークバーを動かした際のリアルタイムプレビューが実現する
    ApplyCameraTransform_();
}

void RailCameraComponent::ComputePositionAndTarget_(float progress, Vector3& outPos, Vector3& outTarget) const {
    // progress は弧長ベースの進行割合(0-1)。スプラインの生パラメータtへ変換してから座標を求める
    const float totalLength = arcLengthTable_.GetTotalLength();
    const float t = (totalLength > 0.0f) ? arcLengthTable_.GetTAtDistance(progress * totalLength) : progress;

    // 現在地点の計算
    outPos = Spline::GetPoint(waypoints_, t);

    // 注視点の計算 (Look-ahead)：弧長ベースで一定割合先の地点を見るようにする
    if (progress >= 1.0f - 0.001f) {
        // 終点付近では、少し手前の点から終点への方向を向くようにする
        const float sBeforeEnd = std::max(1.0f - lookAheadOffset_, 0.0f);
        const float tBeforeEnd = (totalLength > 0.0f) ? arcLengthTable_.GetTAtDistance(sBeforeEnd * totalLength) : sBeforeEnd;
        Vector3 p1 = Spline::GetPoint(waypoints_, tBeforeEnd);
        Vector3 p2 = Spline::GetPoint(waypoints_, 1.0f);
        Vector3 diff = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
        outTarget = { outPos.x + diff.x, outPos.y + diff.y, outPos.z + diff.z };
    } else {
        const float sTarget = std::min(progress + lookAheadOffset_, 1.0f);
        const float tTarget = (totalLength > 0.0f) ? arcLengthTable_.GetTAtDistance(sTarget * totalLength) : sTarget;
        outTarget = Spline::GetPoint(waypoints_, tTarget);
    }
}

void RailCameraComponent::GetPointAndForward(float progress, Vector3& outPos, Vector3& outForward) const {
    if (waypoints_.size() < 2) {
        outPos = { 0.0f, 0.0f, 0.0f };
        outForward = { 0.0f, 0.0f, 1.0f };
        return;
    }
    progress = std::clamp(progress, 0.0f, 1.0f);

    Vector3 targetPos;
    ComputePositionAndTarget_(progress, outPos, targetPos);

    Vector3 diff = { targetPos.x - outPos.x, targetPos.y - outPos.y, targetPos.z - outPos.z };
    outForward = Normalize(diff);
}

void RailCameraComponent::ApplyCameraTransform_() {
    if (waypoints_.size() < 2) return;
    auto* scene = BaseScene::GetActiveScene();
    if (!scene) return;
    auto* camera = scene->GetMainCamera();
    if (!camera) return;

    Vector3 currentPos, targetPos;
    ComputePositionAndTarget_(progress_, currentPos, targetPos);

    // カメラの設定
    camera->SetEye(currentPos);
    camera->SetTarget(targetPos);
    camera->SetUp({ 0.0f, 1.0f, 0.0f });

    // タイムラインシーク時のプレイヤー位置の同期（エディットモードでのシーク時のみ）
    if (scene->GetPlayMode() == PlayMode::Edit) {
        const auto& rootObjs = scene->GetRootObjects();
        for (size_t i = 0; i < rootObjs.size(); ++i) {
            auto obj = rootObjs[i];
            if (!obj) continue;
            if (auto pComp = obj->GetComponent<PlayerComponent>()) {
                pComp->Update(0.0f);
            }
        }
    }
}


void RailCameraComponent::RebuildArcLengthTable_() {
    arcLengthTable_.Build(waypoints_);
}

void RailCameraComponent::Serialize(nlohmann::json& j) const {
    nlohmann::json waypointsArray = nlohmann::json::array();
    for (const auto& pt : waypoints_) {
        waypointsArray.push_back({ {"x", pt.x}, {"y", pt.y}, {"z", pt.z} });
    }
    j["waypoints"] = waypointsArray;
}

void RailCameraComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("waypoints") && j["waypoints"].is_array()) {
        waypoints_.clear();
        for (const auto& ptJson : j["waypoints"]) {
            waypoints_.push_back({
                ptJson.value("x", 0.0f),
                ptJson.value("y", 0.0f),
                ptJson.value("z", 0.0f)
            });
        }
    }

    // デシリアライズ後、selectedPointIndex_ が範囲外にならないようリセットする
    // （空配列をロードした場合や要素数が減った場合の範囲外アクセスを防止）
    if (waypoints_.empty()) {
        selectedPointIndex_ = -1;
    } else if (selectedPointIndex_ >= static_cast<int>(waypoints_.size())) {
        selectedPointIndex_ = static_cast<int>(waypoints_.size()) - 1;
    }

    RebuildArcLengthTable_();
}

void RailCameraComponent::AddWaypoint(const Vector3& pos) {
    nlohmann::json beforeState;
    Serialize(beforeState);

    waypoints_.push_back(pos);
    isModified_ = true;
    RebuildArcLengthTable_();

    nlohmann::json afterState;
    Serialize(afterState);

#ifdef USE_IMGUI
    if (owner_) {
        if (auto* cmdMgr = BaseScene::GetActiveScene()->GetCommandManager()) {
            cmdMgr->AddCommand(std::make_shared<AbsoluteEngine::ComponentStateCommand>(
                owner_->shared_from_this(), GetTypeName(), beforeState, afterState));
        }
    }
#endif
}

void RailCameraComponent::InsertWaypoint(size_t index, const Vector3& pos) {
    if (index <= waypoints_.size()) {
        nlohmann::json beforeState;
        Serialize(beforeState);

        waypoints_.insert(waypoints_.begin() + index, pos);
        isModified_ = true;
        RebuildArcLengthTable_();

        nlohmann::json afterState;
        Serialize(afterState);

#ifdef USE_IMGUI
        if (owner_) {
            if (auto* cmdMgr = BaseScene::GetActiveScene()->GetCommandManager()) {
                cmdMgr->AddCommand(std::make_shared<AbsoluteEngine::ComponentStateCommand>(
                    owner_->shared_from_this(), GetTypeName(), beforeState, afterState));
            }
        }
#endif
    }
}

void RailCameraComponent::RemoveWaypoint(size_t index) {
    if (index < waypoints_.size() && waypoints_.size() > 3) {
        nlohmann::json beforeState;
        Serialize(beforeState);

        waypoints_.erase(waypoints_.begin() + index);
        if (selectedPointIndex_ >= static_cast<int>(waypoints_.size())) {
            selectedPointIndex_ = static_cast<int>(waypoints_.size()) - 1;
        }
        isModified_ = true;
        RebuildArcLengthTable_();

        nlohmann::json afterState;
        Serialize(afterState);

#ifdef USE_IMGUI
        if (owner_) {
            if (auto* cmdMgr = BaseScene::GetActiveScene()->GetCommandManager()) {
                cmdMgr->AddCommand(std::make_shared<AbsoluteEngine::ComponentStateCommand>(
                    owner_->shared_from_this(), GetTypeName(), beforeState, afterState));
            }
        }
#endif
    }
}

void RailCameraComponent::DrawInspectorUI() {
#ifdef USE_IMGUI
    ImGui::Text("Waypoints: %d", static_cast<int>(waypoints_.size()));

    // 秒速ユニット。弧長ベースでレールを等速移動する速度（Durationはここから自動算出される）
    if (ImGui::DragFloat("Speed (units/s)", &speed_, 0.1f, 0.1f, 200.0f, "%.1f")) {
        isModified_ = true;
    }
    ImGui::Text("Rail Length: %.1f units", GetTotalLength());
    ImGui::Text("Duration: %.2f s", GetDuration());

    if (ImGui::Button("Add Point")) {
        Vector3 newPos = waypoints_.empty() ? Vector3(0,0,0) : waypoints_.back();
        newPos.x += 10.0f;
        AddWaypoint(newPos);
        selectedPointIndex_ = static_cast<int>(waypoints_.size()) - 1;
    }

    ImGui::Separator();
    ImGui::Text("Points List (Right-Click for menu):");

    for (int i = 0; i < static_cast<int>(waypoints_.size()); ++i) {
        std::string label = "Point " + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), selectedPointIndex_ == i)) {
            selectedPointIndex_ = i;
        }

        if (ImGui::BeginPopupContextItem(("PointContextMenu" + std::to_string(i)).c_str(), ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Insert After")) {
                Vector3 newPos = waypoints_[i];
                newPos.z += 5.0f;
                InsertWaypoint(i + 1, newPos);
            }
            if (ImGui::MenuItem("Delete")) {
                RemoveWaypoint(i);
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    if (selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(waypoints_.size())) {
        ImGui::Separator();
        ImGui::Text("Selected Point: %d", selectedPointIndex_);
        
        float p[3] = { waypoints_[selectedPointIndex_].x, waypoints_[selectedPointIndex_].y, waypoints_[selectedPointIndex_].z };
        if (ImGui::DragFloat3("Position", p, 0.1f)) {
            waypoints_[selectedPointIndex_] = { p[0], p[1], p[2] };
            isModified_ = true;
            RebuildArcLengthTable_();
        }
        if (ImGui::IsItemActivated()) {
            waypointsBeforeEdit_ = waypoints_;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (owner_) {
                if (auto* cmdMgr = BaseScene::GetActiveScene()->GetCommandManager()) {
                    nlohmann::json beforeState, afterState;
                    auto tmp = waypoints_;
                    waypoints_ = waypointsBeforeEdit_;
                    Serialize(beforeState);
                    waypoints_ = tmp;
                    Serialize(afterState);

                    cmdMgr->AddCommand(std::make_shared<AbsoluteEngine::ComponentStateCommand>(
                        owner_->shared_from_this(), GetTypeName(), beforeState, afterState));
                }
            }
        }
    }


#endif
}

void RailCameraComponent::DrawGizmo(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowPosX, float windowPosY, float windowSizeX, float windowSizeY) {
#ifdef USE_IMGUI
    if (selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(waypoints_.size())) {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(windowPosX, windowPosY, windowSizeX, windowSizeY);

        Matrix4x4 pointTransform = MakeTranslateMatrix(waypoints_[selectedPointIndex_]);
        float objectMatrix[16];
        memcpy(objectMatrix, &pointTransform.m[0][0], sizeof(float) * 16);

        bool wasGizmoUsing = isGizmoUsing_;
        isGizmoUsing_ = ImGuizmo::Manipulate(
            &viewMatrix.m[0][0], 
            &projectionMatrix.m[0][0],
            ImGuizmo::TRANSLATE, 
            ImGuizmo::WORLD, 
            objectMatrix
        );

        if (!wasGizmoUsing && isGizmoUsing_) {
            waypointsBeforeEdit_ = waypoints_;
        }

        if (isGizmoUsing_) {
            float matrixTranslation[3], matrixRotation[3], matrixScale[3];
            ImGuizmo::DecomposeMatrixToComponents(objectMatrix, matrixTranslation, matrixRotation, matrixScale);
            
            Vector3 newPos = { matrixTranslation[0], matrixTranslation[1], matrixTranslation[2] };
            if (waypoints_[selectedPointIndex_].x != newPos.x ||
                waypoints_[selectedPointIndex_].y != newPos.y ||
                waypoints_[selectedPointIndex_].z != newPos.z) {
                waypoints_[selectedPointIndex_] = newPos;
                RebuildArcLengthTable_();
            }
        }

        if (wasGizmoUsing && !isGizmoUsing_) {
            isModified_ = true;
            if (owner_) {
                if (auto* cmdMgr = BaseScene::GetActiveScene()->GetCommandManager()) {
                    nlohmann::json beforeState, afterState;
                    auto tmp = waypoints_;
                    waypoints_ = waypointsBeforeEdit_;
                    Serialize(beforeState);
                    waypoints_ = tmp;
                    Serialize(afterState);

                    cmdMgr->AddCommand(std::make_shared<AbsoluteEngine::ComponentStateCommand>(
                        owner_->shared_from_this(), GetTypeName(), beforeState, afterState));
                }
            }
        }
    }
#endif
}

void RailCameraComponent::HandleMousePicking(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowPosX, float windowPosY, float windowSizeX, float windowSizeY) {
#ifdef USE_IMGUI
    if (!ImGui::IsWindowHovered() || ImGuizmo::IsOver() || isGizmoUsing_) return;

    if (ImGui::IsMouseClicked(0)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        float u = (mousePos.x - windowPosX) / windowSizeX;
        float v = (mousePos.y - windowPosY) / windowSizeY;
        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return;

        float ndcX = u * 2.0f - 1.0f;
        float ndcY = 1.0f - v * 2.0f;

        Matrix4x4 viewProj = Multiply(viewMatrix, projectionMatrix);
        Matrix4x4 invViewProj = Inverse(viewProj);

        Vector3 nearPos = TransformPoint({ ndcX, ndcY, 0.0f }, invViewProj);
        Vector3 farPos = TransformPoint({ ndcX, ndcY, 1.0f }, invViewProj);
        Vector3 rayDir = Normalize({ farPos.x - nearPos.x, farPos.y - nearPos.y, farPos.z - nearPos.z });

        float closestDist = -1.0f;
        int hitIndex = -1;
        float pointRadius = 2.0f; 

        for (int i = 0; i < static_cast<int>(waypoints_.size()); ++i) {
            Vector3 m = { nearPos.x - waypoints_[i].x, nearPos.y - waypoints_[i].y, nearPos.z - waypoints_[i].z };
            float b = Dot(m, rayDir);
            float c = Dot(m, m) - pointRadius * pointRadius;
            
            if (c > 0.0f && b > 0.0f) continue;
            
            float discriminant = b * b - c;
            if (discriminant >= 0.0f) {
                float t = -b - std::sqrt(discriminant);
                if (t < 0.0f) t = 0.0f;
                if (closestDist < 0.0f || t < closestDist) {
                    closestDist = t;
                    hitIndex = i;
                }
            }
        }

        if (hitIndex != -1) {
            selectedPointIndex_ = hitIndex;
        }
    }
#endif
}
