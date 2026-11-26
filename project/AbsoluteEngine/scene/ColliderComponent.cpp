#include "ColliderComponent.h"

#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"
#endif

namespace AbsoluteEngine {

#ifdef USE_IMGUI
namespace {
// インスペクタのドラッグの刻み(以前 EditorUIManager に書かれていた値と同じ)
constexpr float kDragStep = 0.1f;
// 形状の表示名。Type の並び(None, Sphere, AABB)と同じ順にする
constexpr const char* kTypeNames[] = { "None", "Sphere", "AABB" };
} // namespace
#endif

void ColliderComponent::DrawInspectorUI() {
#ifdef USE_IMGUI
    // 設定のコピーを編集し、最後に SetSettings で 1 回だけ戻す(書き込みの窓口を 1 つに保つため)
    Settings collider = settings_;

    int currentType = static_cast<int>(collider.type);
    if (ImGui::Combo("Type", &currentType, kTypeNames, IM_ARRAYSIZE(kTypeNames))) {
        collider.type = static_cast<Type>(currentType);
    }

    // 形状ごとに意味のある大きさの項目だけを出す
    if (collider.type != Type::None) {
        ImGui::DragFloat3("Center Offset", &collider.centerOffset.x, kDragStep);

        if (collider.type == Type::Sphere) {
            ImGui::DragFloat("Radius", &collider.radius, kDragStep, 0.0f);
        } else if (collider.type == Type::AABB) {
            ImGui::DragFloat3("Size (Half Extents)", &collider.size.x, kDragStep, 0.0f);
        }
    }

    SetSettings(collider);
#endif
}

} // namespace AbsoluteEngine
