#include "FormationSpawnEvent.h"
#include "StraightMoveComponent.h"
#include "FormationMemberComponent.h"
#include "AbsoluteEngine/scene/timeline/PrefabRegistry.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/SerialNumberIssuer.h"
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
    // 編隊の中心の Transform を保存する JSON のキー(既存のステージのデータと同じ)
    constexpr const char* kTransformKey = "basePosition";
}

std::string FormationSpawnEvent::GetLabel() const {
    return "Formation: " + (GetPrefabId().empty() ? "(未設定)" : GetPrefabId()) +
           " x" + std::to_string(std::clamp(memberCount_, 3, 5));
}

void FormationSpawnEvent::Fire(const AbsoluteEngine::TimelineFireContext& context) {
    // 既にスポーン済みの場合はスキップ（二重スポーン防止。TimelineTrackの発火済み管理が主ガードだが
    // SpawnEventと同様に念のためのガードも入れておく）
    if (!spawnedObjects_.empty()) {
        return;
    }

    const std::string path = AbsoluteEngine::PrefabRegistry::GetInstance().ResolveId(GetPrefabId());
    if (path.empty()) {
        std::cerr << "[FormationSpawnEvent] プレハブIDを解決できません: \"" << GetPrefabId() << "\"" << std::endl;
        return;
    }

    // 文脈で渡された窓口の束から、シーンへ追加する(プレハブを読むときも、同じ束を使う)
    if (!context.deps || !context.deps->spawner || !context.deps->serialNumbers) {
        std::cerr << "[FormationSpawnEvent] オブジェクトを出す窓口、または通し番号の発行役が設定されていません" << std::endl;
        return;
    }

    // 編隊メンバーをまとめて識別するためのID。Fire()のたびに新しい編隊として、シーンの世界が持つ発行役から採番する。
    const int formationId = context.deps->serialNumbers->Issue();

    // 編隊の中心位置(世界座標)。レール座標の指定なら、発火時刻のレール位置から変換する。
    // 散らす向きは、レール座標のときは道の右・上方向、世界座標のときは世界の X・Y 軸
    Transform basePlaced{};
    AbsoluteEngine::RailFrame railFrame{};
    const bool placedOnRail = ResolveSpawnTransform(context, basePlaced, railFrame);
    const Vector3 spreadAxisX = placedOnRail ? railFrame.right : kWorldAxisX;
    const Vector3 spreadAxisY = placedOnRail ? railFrame.up : kWorldAxisY;

    const int count = std::clamp(memberCount_, 3, 5);
    for (int i = 0; i < count; ++i) {
        auto obj = AbsoluteEngine::SceneSerializer::LoadPrefab(path, *context.deps);
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
        obj->SetTransform(t);

        // 移動（既存Enemy.jsonにはStraightMoveComponentが含まれないため明示的に付与する）
        obj->AddComponent(std::make_unique<StraightMoveComponent>());
        // 編隊識別（GameScene側の全滅ボーナス判定用。JSON経由では作らないプログラム的な付与）
        obj->AddComponent(std::make_unique<FormationMemberComponent>(formationId));

        // レール上に出したことを、コンポーネントへ知らせる(移動の向きなどを、レールの進行方向に合わせる)。
        // 移動の部品を付けたあとに呼ぶので、StraightMoveComponent にも届く
        if (placedOnRail) {
            AbsoluteEngine::NotifySpawnedOnRail(*obj, railFrame);
        }

        context.deps->spawner->Spawn(obj);
        spawnedObjects_.push_back(obj);
    }

    std::cout << "[FormationSpawnEvent] 編隊スポーン完了: \"" << GetPrefabId() << "\" x" << count << std::endl;
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
    // プレハブ ID と編隊の中心(従来のキー basePosition)は、中間クラスが書く
    SerializeSpawnSettings(j, kTransformKey);
    j["memberCount"] = memberCount_;
    j["clusterRadius"] = clusterRadius_;
}

void FormationSpawnEvent::Deserialize(const nlohmann::json& j) {
    DeserializeSpawnSettings(j, kTransformKey);
    memberCount_ = std::clamp(j.value("memberCount", 4), 3, 5);
    clusterRadius_ = j.value("clusterRadius", 3.0f);
}

void FormationSpawnEvent::DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    // プレハブID選択(コンボは中間クラスが描く)
    DrawPrefabSelectUI("Prefab##Formation", activated, deactivatedAfterEdit);

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
    DrawPlacementUI("##Formation", activated, deactivatedAfterEdit);

    ImGui::Separator();
#endif
}
