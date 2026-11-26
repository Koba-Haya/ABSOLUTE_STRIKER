// ============================================================
// SpawnEvent.cpp
// プレハブスポーンイベントの実装
// ============================================================
#include "SpawnEvent.h"
#include "PrefabRegistry.h"
#include "../SceneSerializer.h"
#include "../ISpawner.h"
#include "../GameObject.h"
#include <iostream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AbsoluteEngine {

namespace {
// 置き場所の Transform を保存する JSON のキー(既存のステージのデータと同じ)
constexpr const char* kTransformKey = "spawnTransform";
} // namespace

std::string SpawnEvent::GetLabel() const {
    return "Spawn: " + (GetPrefabId().empty() ? "(未設定)" : GetPrefabId());
}

void SpawnEvent::Fire(const TimelineFireContext& context) {
    // 既にスポーン済みのオブジェクトが存在する場合はスキップ（二重スポーン防止）
    if (!spawnedObject_.expired()) {
        return;
    }

    // 文脈で渡された窓口の束が無ければ、プレハブを読めない(部品を作れない)し、シーンへも出せない
    if (!context.deps || !context.deps->spawner) {
        std::cerr << "[SpawnEvent] オブジェクトを出す窓口が設定されていません" << std::endl;
        return;
    }

    // PrefabRegistryからIDでファイルパスを解決する
    const std::string path = PrefabRegistry::GetInstance().ResolveId(GetPrefabId());
    if (path.empty()) {
        std::cerr << "[SpawnEvent] プレハブIDを解決できません: \"" << GetPrefabId() << "\"" << std::endl;
        return;
    }

    // プレハブをロードしてオブジェクトを生成する
    auto obj = SceneSerializer::LoadPrefab(path, *context.deps);
    if (!obj) {
        std::cerr << "[SpawnEvent] プレハブのロードに失敗しました: " << path << std::endl;
        return;
    }

    // スポーン位置に配置する(レール座標の指定なら、発火時刻のレール上の位置から世界座標へ変換する)
    RailFrame railFrame{};
    Transform placed{};
    const bool placedOnRail = ResolveSpawnTransform(context, placed, railFrame);
    obj->SetTransform(placed);
    // レール上に出したことを、コンポーネントへ知らせる(進行方向に依存する動きが、向きを決める)
    if (placedOnRail) {
        NotifySpawnedOnRail(*obj, railFrame);
    }

    // 文脈で渡された窓口から、シーンへ追加する。shared_ptr のまま追加し、weak_ptr で参照を保持する
    context.deps->spawner->Spawn(obj);
    spawnedObject_ = obj;

    std::cout << "[SpawnEvent] スポーン完了: \"" << GetPrefabId() << "\"" << std::endl;
}

void SpawnEvent::Rewind() {
    // weak_ptr でオブジェクトの生存を確認してから破棄する
    // （expired() が true の場合はすでに別の手段で破棄されているので何もしない）
    if (auto ptr = spawnedObject_.lock()) {
        ptr->Destroy();
        std::cout << "[SpawnEvent] Rewind: \"" << GetPrefabId() << "\" を破棄しました" << std::endl;
    }

    // weak_ptr をリセットして次のFire()に備える
    spawnedObject_.reset();
}

void SpawnEvent::Serialize(nlohmann::json& j) const {
    // プレハブ ID と置き場所(従来のキー spawnTransform)は、中間クラスが書く
    SerializeSpawnSettings(j, kTransformKey);
}

void SpawnEvent::Deserialize(const nlohmann::json& j) {
    DeserializeSpawnSettings(j, kTransformKey);
}

void SpawnEvent::DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) {
#ifdef USE_IMGUI
    // プレハブID（表示のみ、変更不可）
    ImGui::Text("Prefab: %s", GetPrefabId().c_str());

    ImGui::Separator();

    // スポーン位置 (Spawn Offset) の編集(レール座標か世界座標か、位置、回転、スケール。欄は中間クラスが描く)
    ImGui::Text("Spawn Offset:");
    DrawPlacementUI("##SpawnOffset", activated, deactivatedAfterEdit);

    ImGui::Separator();
#endif
}

void SpawnEvent::DrawCreationUI(bool& activated, bool& deactivatedAfterEdit) {
    // 作成時のみプレハブIDを選択可能にする（配置後はDrawInspectorUI()側で表示のみになる）
    DrawPrefabSelectUI("プレハブID", activated, deactivatedAfterEdit);
}

} // namespace AbsoluteEngine
