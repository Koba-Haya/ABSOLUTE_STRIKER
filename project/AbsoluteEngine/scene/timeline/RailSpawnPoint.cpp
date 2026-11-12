// ============================================================
// RailSpawnPoint.cpp
// レール座標での出現位置の実装
// ============================================================
#include "RailSpawnPoint.h"
#include "../GameObject.h"
#include "../Component.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AbsoluteEngine {

namespace {
    // JSON の項目名
    constexpr const char* kAheadKey = "ahead";
    constexpr const char* kLateralKey = "lateral";
    constexpr const char* kVerticalKey = "vertical";

#ifdef USE_IMGUI
    // エディタでの入力のつまみ 1 ステップあたりの量(ユニット)
    constexpr float kRailDragSpeed = 0.1f;
#endif
}

nlohmann::json RailSpawnPointToJson(const RailSpawnPoint& rail) {
    return {
        { kAheadKey, rail.ahead },
        { kLateralKey, rail.lateral },
        { kVerticalKey, rail.vertical }
    };
}

RailSpawnPoint RailSpawnPointFromJson(const nlohmann::json& j) {
    RailSpawnPoint rail;
    rail.ahead = j.value(kAheadKey, 0.0f);
    rail.lateral = j.value(kLateralKey, 0.0f);
    rail.vertical = j.value(kVerticalKey, 0.0f);
    return rail;
}

bool ResolveRailTransform(const RailSpawnPoint& rail, const TimelineFireContext& context,
                          Transform& inOutTransform, RailFrame& outFrame) {
    if (context.camera == nullptr) {
        return false;
    }

    // 発火時刻のレール上の距離から、ahead だけ先の地点の座標系を求める。
    // 始点より前・終点より先でも、GetRailFrame は直線で外挿して値を返す
    outFrame = context.camera->GetRailFrame(context.GetRailDistance() + rail.ahead);

    // 世界座標 = 位置 + 右 x lateral + 上 x vertical
    inOutTransform.translate = {
        outFrame.position.x + outFrame.right.x * rail.lateral + outFrame.up.x * rail.vertical,
        outFrame.position.y + outFrame.right.y * rail.lateral + outFrame.up.y * rail.vertical,
        outFrame.position.z + outFrame.right.z * rail.lateral + outFrame.up.z * rail.vertical
    };
    return true;
}

void NotifySpawnedOnRail(GameObject& object, const RailFrame& frame) {
    for (const auto& component : object.GetComponents()) {
        if (component) {
            component->OnSpawnedOnRail(frame);
        }
    }
}

void DrawRailSpawnPointUI(bool& useRail, RailSpawnPoint& rail, bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    ImGui::Checkbox("Use Rail Coordinates", &useRail);
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    if (useRail) {
        float values[3] = { rail.ahead, rail.lateral, rail.vertical };
        if (ImGui::DragFloat3("Rail (ahead, lateral, vertical)", values, kRailDragSpeed)) {
            rail.ahead = values[0];
            rail.lateral = values[1];
            rail.vertical = values[2];
        }
        activated |= ImGui::IsItemActivated();
        deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
    }
#endif
}

} // namespace AbsoluteEngine
