#include "FormationSpawnEvent.h"
#include "StraightMoveComponent.h"
#include "FormationMemberComponent.h"
#include "AbsoluteEngine/scene/timeline/PrefabRegistry.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include <algorithm>
#include <cstdlib> // rand()
#include <iostream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    // 世界座標の指定のとき、編隊を散らす軸(従来どおり世界の X・Y 軸)
    constexpr Vector3 kWorldAxisX = { 1.0f, 0.0f, 0.0f };
    constexpr Vector3 kWorldAxisY = { 0.0f, 1.0f, 0.0f };
}

// JSON変換用ヘルパー（SpawnEvent.cppの同名ローカル定義と同じ形。ファイル単位で完結させる）
static nlohmann::json TransformToJson(const Transform& t) {
    return {
        {"translate", {{"x", t.translate.x}, {"y", t.translate.y}, {"z", t.translate.z}}},
        {"rotate",    {{"x", t.rotate.x},    {"y", t.rotate.y},    {"z", t.rotate.z}}},
        {"scale",     {{"x", t.scale.x},     {"y", t.scale.y},     {"z", t.scale.z}}}
    };
}

static Transform JsonToTransform(const nlohmann::json& j) {
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

std::string FormationSpawnEvent::GetLabel() const {
    return "Formation: " + (prefabId_.empty() ? "(未設定)" : prefabId_) +
           " x" + std::to_string(std::clamp(memberCount_, 3, 5));
}

void FormationSpawnEvent::Fire(const AbsoluteEngine::TimelineFireContext& context) {
    // 既にスポーン済みの場合はスキップ（二重スポーン防止。TimelineTrackの発火済み管理が主ガードだが
    // SpawnEventと同様に念のためのガードも入れておく）
    if (!spawnedObjects_.empty()) {
        return;
    }

    const std::string path = AbsoluteEngine::PrefabRegistry::GetInstance().ResolveId(prefabId_);
    if (path.empty()) {
        std::cerr << "[FormationSpawnEvent] プレハブIDを解決できません: \"" << prefabId_ << "\"" << std::endl;
        return;
    }

    BaseScene* activeScene = BaseScene::GetActiveScene();
    if (!activeScene) {
        std::cerr << "[FormationSpawnEvent] アクティブシーンが存在しません" << std::endl;
        return;
    }

    // 編隊メンバーをまとめて識別するためのID。Fire()のたびに新しい編隊として採番する。
    static int nextFormationId = 0;
    const int formationId = nextFormationId++;

    // 編隊の中心位置(世界座標)。レール座標の指定なら、発火時刻のレール位置から変換する。
    // 散らす向きは、レール座標のときは道の右・上方向、世界座標のときは世界の X・Y 軸
    Transform basePlaced = basePosition_;
    AbsoluteEngine::RailFrame railFrame{};
    const bool placedOnRail = useRail_ && AbsoluteEngine::ResolveRailTransform(rail_, context, basePlaced, railFrame);
    const Vector3 spreadAxisX = placedOnRail ? railFrame.right : kWorldAxisX;
    const Vector3 spreadAxisY = placedOnRail ? railFrame.up : kWorldAxisY;

    const int count = std::clamp(memberCount_, 3, 5);
    for (int i = 0; i < count; ++i) {
        auto obj = AbsoluteEngine::SceneSerializer::LoadPrefab(path);
        if (!obj) {
            std::cerr << "[FormationSpawnEvent] プレハブのロードに失敗しました: " << path << std::endl;
            continue;
        }

        // クラスタ配置: 基準位置からXY平面(レール座標なら道の右・上方向)上にランダムオフセットして散らす
        Transform t = basePlaced;
        const float offsetX = ((rand() % 200) / 100.0f - 1.0f) * clusterRadius_;
        const float offsetY = ((rand() % 200) / 100.0f - 1.0f) * clusterRadius_;
        t.translate.x += spreadAxisX.x * offsetX + spreadAxisY.x * offsetY;
        t.translate.y += spreadAxisX.y * offsetX + spreadAxisY.y * offsetY;
        t.translate.z += spreadAxisX.z * offsetX + spreadAxisY.z * offsetY;
        obj->GetTransform() = t;

        // 移動（既存Enemy.jsonにはStraightMoveComponentが含まれないため明示的に付与する）
        obj->AddComponent(std::make_unique<StraightMoveComponent>());
        // 編隊識別（GameScene側の全滅ボーナス判定用。JSON経由では作らないプログラム的な付与）
        obj->AddComponent(std::make_unique<FormationMemberComponent>(formationId));

        // レール上に出したことを、コンポーネントへ知らせる(移動の向きなどを、レールの進行方向に合わせる)。
        // 移動の部品を付けたあとに呼ぶので、StraightMoveComponent にも届く
        if (placedOnRail) {
            AbsoluteEngine::NotifySpawnedOnRail(*obj, railFrame);
        }

        activeScene->AddRootObject(obj);
        spawnedObjects_.push_back(obj);
    }

    std::cout << "[FormationSpawnEvent] 編隊スポーン完了: \"" << prefabId_ << "\" x" << count << std::endl;
}

void FormationSpawnEvent::Rewind() {
    for (auto& weak : spawnedObjects_) {
        if (auto ptr = weak.lock()) {
            ptr->Destroy();
        }
    }
    spawnedObjects_.clear();
}

void FormationSpawnEvent::Serialize(nlohmann::json& j) const {
    j["prefabId"] = prefabId_;
    j["basePosition"] = TransformToJson(basePosition_);
    // レール座標の指定があるときだけ書く(無ければ、従来と同じ形の JSON になる)
    if (useRail_) {
        j["rail"] = AbsoluteEngine::RailSpawnPointToJson(rail_);
    }
    j["memberCount"] = memberCount_;
    j["clusterRadius"] = clusterRadius_;
}

void FormationSpawnEvent::Deserialize(const nlohmann::json& j) {
    prefabId_ = j.value("prefabId", "");
    if (j.contains("basePosition")) {
        basePosition_ = JsonToTransform(j["basePosition"]);
    }
    // 「rail」項目があればレール座標の指定。無ければ従来の世界座標(後方互換)
    useRail_ = j.contains("rail") && j["rail"].is_object();
    rail_ = useRail_ ? AbsoluteEngine::RailSpawnPointFromJson(j["rail"]) : AbsoluteEngine::RailSpawnPoint{};
    memberCount_ = std::clamp(j.value("memberCount", 4), 3, 5);
    clusterRadius_ = j.value("clusterRadius", 3.0f);
}

void FormationSpawnEvent::DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    // プレハブID選択
    const std::vector<std::string> prefabIds = AbsoluteEngine::PrefabRegistry::GetInstance().GetAllIds();
    if (prefabIds.empty()) {
        ImGui::TextColored({ 1.0f, 0.5f, 0.5f, 1.0f }, "プレハブが登録されていません。");
    } else {
        int currentIndex = 0;
        std::vector<const char*> idCStrs;
        idCStrs.reserve(prefabIds.size());
        for (size_t i = 0; i < prefabIds.size(); ++i) {
            idCStrs.push_back(prefabIds[i].c_str());
            if (prefabIds[i] == prefabId_) currentIndex = static_cast<int>(i);
        }
        if (ImGui::Combo("Prefab##Formation", &currentIndex, idCStrs.data(), static_cast<int>(idCStrs.size()))) {
            prefabId_ = prefabIds[static_cast<size_t>(currentIndex)];
        }
        activated |= ImGui::IsItemActivated();
        deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
    }

    ImGui::Separator();

    // 編隊メンバー数（3〜5にクランプ）
    if (ImGui::InputInt("Member Count##Formation", &memberCount_)) {
        memberCount_ = std::clamp(memberCount_, 3, 5);
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    // クラスタ配置のばらつき半径
    ImGui::DragFloat("Cluster Radius##Formation", &clusterRadius_, 0.1f, 0.0f, 50.0f);
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::Separator();

    // 編隊の中心位置(レール座標か、世界座標か)
    ImGui::Text("Base Position:");
    AbsoluteEngine::DrawRailSpawnPointUI(useRail_, rail_, activated, deactivatedAfterEdit);
    if (!useRail_) {
        float pos[3] = { basePosition_.translate.x, basePosition_.translate.y, basePosition_.translate.z };
        if (ImGui::DragFloat3("Position##Formation", pos, 0.1f)) {
            basePosition_.translate = { pos[0], pos[1], pos[2] };
        }
        activated |= ImGui::IsItemActivated();
        deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
    }

    float rot[3] = { basePosition_.rotate.x, basePosition_.rotate.y, basePosition_.rotate.z };
    if (ImGui::DragFloat3("Rotation##Formation", rot, 0.01f)) {
        basePosition_.rotate = { rot[0], rot[1], rot[2] };
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    float scl[3] = { basePosition_.scale.x, basePosition_.scale.y, basePosition_.scale.z };
    if (ImGui::DragFloat3("Scale##Formation", scl, 0.1f, 0.001f, 100.0f)) {
        basePosition_.scale = { scl[0], scl[1], scl[2] };
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::Separator();
#endif
}
