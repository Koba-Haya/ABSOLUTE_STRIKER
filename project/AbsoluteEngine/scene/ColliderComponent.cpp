#include "ColliderComponent.h"
#include "GameObject.h"
#include "collision/SphereShape.h"
#include "collision/AabbShape.h"

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

void ColliderComponent::RebuildShape_() {
    // 種類ごとに、形状のクラスを選ぶ(種類の数値で分岐するのは、ここだけ。判定や表示は、形状のクラスに任せる)
    switch (settings_.type) {
    case Type::Sphere:
        shape_ = std::make_unique<SphereShape>(settings_.radius);
        break;
    case Type::AABB:
        shape_ = std::make_unique<AabbShape>(settings_.size);
        break;
    case Type::None:
    default:
        shape_.reset();
        break;
    }
}

ShapePlacement ColliderComponent::GetCollisionPlacement() const {
    ShapePlacement placement;
    const GameObject* owner = GetOwner();
    if (!owner) return placement;
    const Vector3& pos = owner->GetTransform().translate;
    placement.center = { pos.x + settings_.centerOffset.x, pos.y + settings_.centerOffset.y, pos.z + settings_.centerOffset.z };
    // scale は既定の 1 のまま(衝突判定は、オブジェクトの scale を大きさに反映しない)
    return placement;
}

ShapePlacement ColliderComponent::GetEditorPlacement() const {
    ShapePlacement placement;
    const GameObject* owner = GetOwner();
    if (!owner) return placement;
    const Vector3& pos = owner->GetTransform().translate;
    const Vector3& scale = owner->GetTransform().scale;
    placement.center = {
        pos.x + settings_.centerOffset.x * scale.x,
        pos.y + settings_.centerOffset.y * scale.y,
        pos.z + settings_.centerOffset.z * scale.z };
    placement.scale = scale;
    return placement;
}

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
