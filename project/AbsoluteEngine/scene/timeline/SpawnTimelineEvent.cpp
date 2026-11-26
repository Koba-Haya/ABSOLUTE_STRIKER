// ============================================================
// SpawnTimelineEvent.cpp
// スポーン系イベントの中間クラスの実装
// ============================================================
#include "SpawnTimelineEvent.h"
#include "PrefabRegistry.h"
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AbsoluteEngine {

namespace {
// 置き場所の編集欄のドラッグの刻みと範囲(以前 SpawnEvent / FormationSpawnEvent に書かれていた値と同じ)
constexpr float kPositionDragStep = 0.1f;
constexpr float kRotationDragStep = 0.01f;
constexpr float kScaleDragStep = 0.1f;
constexpr float kScaleMin = 0.001f;
constexpr float kScaleMax = 100.0f;
// プレハブが 1 つも登録されていないときの警告の色(赤み)
constexpr float kWarningColor[4] = { 1.0f, 0.5f, 0.5f, 1.0f };
// JSON のキー(既存のステージのデータと同じ)
constexpr const char* kPrefabIdKey = "prefabId";
constexpr const char* kRailKey = "rail";

// Transform を JSON へ(以前は SpawnEvent.cpp と FormationSpawnEvent.cpp に同じ関数が 1 つずつあった)
nlohmann::json TransformToJson(const Transform& t) {
    return {
        {"translate", {{"x", t.translate.x}, {"y", t.translate.y}, {"z", t.translate.z}}},
        {"rotate",    {{"x", t.rotate.x},    {"y", t.rotate.y},    {"z", t.rotate.z}}},
        {"scale",     {{"x", t.scale.x},     {"y", t.scale.y},     {"z", t.scale.z}}}
    };
}

// JSON から Transform へ(項目が無ければ、既定値: 位置と回転は 0、スケールは 1)
Transform JsonToTransform(const nlohmann::json& j) {
    Transform t{};
    if (j.contains("translate")) {
        t.translate.x = j["translate"].value("x", 0.0f);
        t.translate.y = j["translate"].value("y", 0.0f);
        t.translate.z = j["translate"].value("z", 0.0f);
    }
    if (j.contains("rotate")) {
        t.rotate.x = j["rotate"].value("x", 0.0f);
        t.rotate.y = j["rotate"].value("y", 0.0f);
        t.rotate.z = j["rotate"].value("z", 0.0f);
    }
    if (j.contains("scale")) {
        t.scale.x = j["scale"].value("x", 1.0f);
        t.scale.y = j["scale"].value("y", 1.0f);
        t.scale.z = j["scale"].value("z", 1.0f);
    }
    return t;
}
} // namespace

bool SpawnTimelineEvent::ResolveSpawnTransform(const TimelineFireContext& context, Transform& outTransform, RailFrame& outFrame) const {
    outTransform = placement_;
    if (!useRail_) {
        return false;
    }
    // レールが無いとき(ResolveRailTransform が false)は、世界座標の placement_ のまま置く
    return ResolveRailTransform(rail_, context, outTransform, outFrame);
}

void SpawnTimelineEvent::OnCreatedInEditor(const RailSpawnPoint& defaultPlacement) {
    useRail_ = true;
    rail_ = defaultPlacement;
}

bool SpawnTimelineEvent::GetEditorHandle(const TimelineFireContext& context, TimelineEditorHandle& outHandle) const {
    // 実際の出現(Fire)と同じ手順で、発火時刻のレール位置から世界座標を求める
    RailFrame unusedFrame{};
    outHandle.prefabId = prefabId_;
    ResolveSpawnTransform(context, outHandle.placement, unusedFrame);
    return true;
}

void SpawnTimelineEvent::OnEditorHandleEdited(const Transform& edited) {
    if (useRail_) {
        // レール座標の指定のときは、位置はレール座標だけで決まる(世界座標を取り込むと、レール座標の指定が崩れる)
        placement_.rotate = edited.rotate;
        placement_.scale = edited.scale;
    } else {
        placement_ = edited;
    }
}

void SpawnTimelineEvent::DrawPrefabSelectUI(const char* label, bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    const std::vector<std::string> prefabIds = PrefabRegistry::GetInstance().GetAllIds();
    if (prefabIds.empty()) {
        ImGui::TextColored({ kWarningColor[0], kWarningColor[1], kWarningColor[2], kWarningColor[3] },
            "プレハブが登録されていません。[Reload Prefabs]を押してください。");
        return;
    }

    // 未設定なら、先頭を選んだ状態にする(コンボの表示と、保存される値をそろえる)
    if (prefabId_.empty()) {
        prefabId_ = prefabIds[0];
    }

    int currentIndex = 0;
    std::vector<const char*> idCStrs;
    idCStrs.reserve(prefabIds.size());
    for (size_t i = 0; i < prefabIds.size(); ++i) {
        idCStrs.push_back(prefabIds[i].c_str());
        if (prefabIds[i] == prefabId_) {
            currentIndex = static_cast<int>(i);
        }
    }

    if (ImGui::Combo(label, &currentIndex, idCStrs.data(), static_cast<int>(idCStrs.size()))) {
        prefabId_ = prefabIds[static_cast<size_t>(currentIndex)];
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
#else
    (void)label;
    (void)activated;
    (void)deactivatedAfterEdit;
#endif
}

void SpawnTimelineEvent::DrawPlacementUI(const char* idSuffix, bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    const std::string suffix = idSuffix;

    // レール座標(前方・横・縦)か、世界座標かを選ぶ。レール座標のときは、世界座標の位置の欄は出さない
    DrawRailSpawnPointUI(useRail_, rail_, activated, deactivatedAfterEdit);
    if (!useRail_) {
        float pos[3] = { placement_.translate.x, placement_.translate.y, placement_.translate.z };
        if (ImGui::DragFloat3(("Position" + suffix).c_str(), pos, kPositionDragStep)) {
            placement_.translate = { pos[0], pos[1], pos[2] };
        }
        activated |= ImGui::IsItemActivated();
        deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
    }

    float rot[3] = { placement_.rotate.x, placement_.rotate.y, placement_.rotate.z };
    if (ImGui::DragFloat3(("Rotation" + suffix).c_str(), rot, kRotationDragStep)) {
        placement_.rotate = { rot[0], rot[1], rot[2] };
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    float scl[3] = { placement_.scale.x, placement_.scale.y, placement_.scale.z };
    if (ImGui::DragFloat3(("Scale" + suffix).c_str(), scl, kScaleDragStep, kScaleMin, kScaleMax)) {
        placement_.scale = { scl[0], scl[1], scl[2] };
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
#else
    (void)idSuffix;
    (void)activated;
    (void)deactivatedAfterEdit;
#endif
}

void SpawnTimelineEvent::SerializeSpawnSettings(nlohmann::json& j, const char* transformKey) const {
    j[kPrefabIdKey] = prefabId_;
    j[transformKey] = TransformToJson(placement_);
    // レール座標の指定があるときだけ書く(無ければ、従来と同じ形の JSON になる)
    if (useRail_) {
        j[kRailKey] = RailSpawnPointToJson(rail_);
    }
}

void SpawnTimelineEvent::DeserializeSpawnSettings(const nlohmann::json& j, const char* transformKey) {
    prefabId_ = j.value(kPrefabIdKey, "");
    if (j.contains(transformKey)) {
        placement_ = JsonToTransform(j[transformKey]);
    }
    // 「rail」項目があればレール座標の指定。無ければ従来の世界座標(後方互換)
    useRail_ = j.contains(kRailKey) && j[kRailKey].is_object();
    rail_ = useRail_ ? RailSpawnPointFromJson(j[kRailKey]) : RailSpawnPoint{};
}

} // namespace AbsoluteEngine
