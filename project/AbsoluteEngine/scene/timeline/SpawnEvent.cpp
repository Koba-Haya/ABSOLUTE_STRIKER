// ============================================================
// SpawnEvent.cpp
// プレハブスポーンイベントの実装
// ============================================================
#include "SpawnEvent.h"
#include "PrefabRegistry.h"
#include "../SceneSerializer.h"
#include "../BaseScene.h"
#include "../GameObject.h"
#include <iostream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// JSON変換用ヘルパー（同一ファイル内ローカル定義）
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

namespace AbsoluteEngine {

std::string SpawnEvent::GetLabel() const {
    return "Spawn: " + (prefabId_.empty() ? "(未設定)" : prefabId_);
}

bool SpawnEvent::ResolveSpawnTransform(const TimelineFireContext& context, Transform& outTransform, RailFrame& outFrame) const {
    outTransform = spawnTransform_;
    if (!useRail_) {
        return false;
    }
    // レールが無いとき(ResolveRailTransform が false)は、世界座標の spawnTransform_ のまま置く
    return ResolveRailTransform(rail_, context, outTransform, outFrame);
}

void SpawnEvent::Fire(const TimelineFireContext& context) {
    // 既にスポーン済みのオブジェクトが存在する場合はスキップ（二重スポーン防止）
    if (!spawnedObject_.expired()) {
        return;
    }

    // PrefabRegistryからIDでファイルパスを解決する
    const std::string path = PrefabRegistry::GetInstance().ResolveId(prefabId_);
    if (path.empty()) {
        std::cerr << "[SpawnEvent] プレハブIDを解決できません: \"" << prefabId_ << "\"" << std::endl;
        return;
    }

    // プレハブをロードしてオブジェクトを生成する
    auto obj = SceneSerializer::LoadPrefab(path);
    if (!obj) {
        std::cerr << "[SpawnEvent] プレハブのロードに失敗しました: " << path << std::endl;
        return;
    }

    // スポーン位置に配置する(レール座標の指定なら、発火時刻のレール上の位置から世界座標へ変換する)
    RailFrame railFrame{};
    Transform placed{};
    const bool placedOnRail = ResolveSpawnTransform(context, placed, railFrame);
    obj->GetTransform() = placed;
    // レール上に出したことを、コンポーネントへ知らせる(進行方向に依存する動きが、向きを決める)
    if (placedOnRail) {
        NotifySpawnedOnRail(*obj, railFrame);
    }

    // アクティブシーンのルートオブジェクトに追加する
    BaseScene* activeScene = BaseScene::GetActiveScene();
    if (!activeScene) {
        std::cerr << "[SpawnEvent] アクティブシーンが存在しません" << std::endl;
        return;
    }

    // shared_ptr のまま rootObjects に追加し、weak_ptr で参照を保持する
    activeScene->AddRootObject(obj);
    spawnedObject_ = obj;

    std::cout << "[SpawnEvent] スポーン完了: \"" << prefabId_ << "\"" << std::endl;
}

void SpawnEvent::Rewind() {
    // weak_ptr でオブジェクトの生存を確認してから破棄する
    // （expired() が true の場合はすでに別の手段で破棄されているので何もしない）
    if (auto ptr = spawnedObject_.lock()) {
        ptr->Destroy();
        std::cout << "[SpawnEvent] Rewind: \"" << prefabId_ << "\" を破棄しました" << std::endl;
    }

    // weak_ptr をリセットして次のFire()に備える
    spawnedObject_.reset();
}

void SpawnEvent::Serialize(nlohmann::json& j) const {
    j["prefabId"] = prefabId_;
    j["spawnTransform"] = TransformToJson(spawnTransform_);
    // レール座標の指定があるときだけ書く(無ければ、従来と同じ形の JSON になる)
    if (useRail_) {
        j["rail"] = RailSpawnPointToJson(rail_);
    }
}

void SpawnEvent::Deserialize(const nlohmann::json& j) {
    prefabId_ = j.value("prefabId", "");
    if (j.contains("spawnTransform")) {
        spawnTransform_ = JsonToTransform(j["spawnTransform"]);
    }
    // 「rail」項目があればレール座標の指定。無ければ従来の世界座標(後方互換)
    useRail_ = j.contains("rail") && j["rail"].is_object();
    rail_ = useRail_ ? RailSpawnPointFromJson(j["rail"]) : RailSpawnPoint{};
}

void SpawnEvent::DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    // プレハブID（表示のみ、変更不可）
    ImGui::Text("Prefab: %s", prefabId_.c_str());

    ImGui::Separator();

    // スポーン位置 (Spawn Offset) の編集 - XYZ個別のDragFloat
    ImGui::Text("Spawn Offset:");
    // レール座標(前方・横・縦)か、世界座標かを選ぶ。レール座標のときは、世界座標の欄は出さない
    DrawRailSpawnPointUI(useRail_, rail_, activated, deactivatedAfterEdit);
    if (!useRail_) {
        float pos[3] = { spawnTransform_.translate.x, spawnTransform_.translate.y, spawnTransform_.translate.z };
        if (ImGui::DragFloat3("Position##SpawnOffset", pos, 0.1f)) {
            spawnTransform_.translate = { pos[0], pos[1], pos[2] };
        }
        activated |= ImGui::IsItemActivated();
        deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
    }

    float rot[3] = { spawnTransform_.rotate.x, spawnTransform_.rotate.y, spawnTransform_.rotate.z };
    if (ImGui::DragFloat3("Rotation##SpawnOffset", rot, 0.01f)) {
        spawnTransform_.rotate = { rot[0], rot[1], rot[2] };
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    float scl[3] = { spawnTransform_.scale.x, spawnTransform_.scale.y, spawnTransform_.scale.z };
    if (ImGui::DragFloat3("Scale##SpawnOffset", scl, 0.1f, 0.001f, 100.0f)) {
        spawnTransform_.scale = { scl[0], scl[1], scl[2] };
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

    ImGui::Separator();
#endif
}

void SpawnEvent::DrawCreationUI(bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    // 作成時のみプレハブIDを選択可能にする（配置後はDrawInspectorUI()側で表示のみになる）
    const std::vector<std::string> prefabIds = PrefabRegistry::GetInstance().GetAllIds();
    if (prefabIds.empty()) {
        ImGui::TextColored({ 1.0f, 0.5f, 0.5f, 1.0f },
            "プレハブが登録されていません。[Reload Prefabs]を押してください。");
        return;
    }

    // 現在のprefabId_に対応するインデックスを毎回探す（このインスタンスは
    // ポップアップ表示中だけ生存する一時オブジェクトのため専用の状態を持つ必要がない）
    int currentIndex = 0;
    if (prefabId_.empty()) {
        prefabId_ = prefabIds[0];
    } else {
        for (size_t i = 0; i < prefabIds.size(); ++i) {
            if (prefabIds[i] == prefabId_) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
    }

    std::vector<const char*> idCStrs;
    idCStrs.reserve(prefabIds.size());
    for (const auto& id : prefabIds) {
        idCStrs.push_back(id.c_str());
    }

    if (ImGui::Combo("プレハブID", &currentIndex, idCStrs.data(), static_cast<int>(idCStrs.size()))) {
        prefabId_ = prefabIds[static_cast<size_t>(currentIndex)];
    }
    activated |= ImGui::IsItemActivated();
    deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
#endif
}

} // namespace AbsoluteEngine
