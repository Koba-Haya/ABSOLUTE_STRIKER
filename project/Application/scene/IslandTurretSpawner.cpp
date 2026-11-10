#include "IslandTurretSpawner.h"

#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "AbsoluteEngine/scene/timeline/PrefabRegistry.h"
#include "StageIsland.h"
#include "StageIslands.h"
#include <iostream>

namespace {
constexpr const char *kTurretPrefabId = "EnemyTurret"; // プレハブの ID(prefabs/EnemyTurret.json の name)
// 島に乗せるタレットの大きさ(立方体の一辺。プレハブの既定は 1。タイムラインの配置は 1.5。島の上では遠くから見えるよう大きくする)
constexpr float kTurretScale = 3.0f;
constexpr float kCubeHalfExtent = 0.5f; // プレハブのモデル(cube.obj)の、半分の大きさ(倍率 1 のとき)。当たり判定の AABB の半幅に使う
// 自機が島へ、レール上の距離でここまで近づいたら、タレットを出す。
// 島は、前方 950 から見える(StageIslandDefaults::kDrawAhead)。タレットは、島の上で急に現れて見えないよう、遠めの 300 で出す
// (立方体の見かけは、300 先で約 0.8 度 = 約 30 画素)。タレットは画面内にいる間だけ撃つので、300 でも撃ち始める。
// 弾の速さは 20(自機の進みは 10)なので、遠い弾は当たらずに通り過ぎ、近づいてから当たるようになる(既存の敵と同じ作り)
constexpr float kSpawnAhead = 300.0f;
} // namespace

void IslandTurretSpawner::Update(BaseScene &scene, const StageIslands &islands, float railDistance) {
  const auto &slots = islands.GetTurretSlots();
  states_.resize(slots.size());

  for (size_t i = 0; i < slots.size(); ++i) {
    SlotState &state = states_[i];
    const float spawnDistance = slots[i].distance - kSpawnAhead;
    if (railDistance < spawnDistance) {
      // まだ近づいていない。出していた(やり直しで手前へ戻った)なら、出し直せる状態に戻す
      state.spawned = false;
      continue;
    }
    if (state.spawned || slots[i].island == nullptr) continue;
    state.spawned = SpawnTurret_(scene, *slots[i].island);
  }
}

bool IslandTurretSpawner::SpawnTurret_(BaseScene &scene, const StageIsland &island) const {
  // タイムラインの SpawnEvent と同じ手順: プレハブ ID をパスへ変えて読み、位置を決めて、シーンへ追加する
  const std::string path = AbsoluteEngine::PrefabRegistry::GetInstance().ResolveId(kTurretPrefabId);
  if (path.empty()) {
    std::cerr << "[IslandTurret] cannot resolve prefab id: " << kTurretPrefabId << std::endl;
    return false;
  }
  auto turret = AbsoluteEngine::SceneSerializer::LoadPrefab(path);
  if (!turret) {
    std::cerr << "[IslandTurret] failed to load prefab: " << path << std::endl;
    return false;
  }

  // 島の上面の頂点(原点から、rock の丘の高さ x 倍率)に、立方体の底が乗る高さへ置く。島は傾けていないので、真上に置ける
  const float halfSize = kCubeHalfExtent * kTurretScale;
  const float topY = island.GetPosition().y + StageIslandDefaults::kRockTopHeight * island.GetScale();
  turret->GetTransform().translate = {island.GetPosition().x, topY + halfSize, island.GetPosition().z};
  turret->GetTransform().scale = {kTurretScale, kTurretScale, kTurretScale};

  // 当たり判定は、倍率を見ない(AABB の半幅をそのまま使う)ので、見た目の大きさに合わせる
  if (auto *collider = turret->GetComponent<AbsoluteEngine::ColliderComponent>()) {
    collider->size = {halfSize, halfSize, halfSize};
  }

  scene.AddRootObject(turret);
  return true;
}
