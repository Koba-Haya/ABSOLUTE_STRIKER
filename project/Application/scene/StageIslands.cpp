#include "StageIslands.h"

#include "../camera/RailCameraComponent.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "Method.h"
#include "ModelResource.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
// 飾りの島のモデル(共有パレット island_palette.png を同じフォルダから読む)。rock は含めない(タレットの島専用)。
// GameOverIslands.cpp にも、同じパスの一覧がある(重複。あちらは無名の名前空間で、参照できないため)
constexpr const char *kDecorModelPaths[] = {
    "resources/app/island/island_small.gltf",
    "resources/app/island/island_medium.gltf",
    "resources/app/island/island_forest.gltf",
};
// タレットの島のモデル(灰色の岩肌。木が無い)
constexpr const char *kTurretModelPath = "resources/app/island/island_rock.gltf";

constexpr float kTwoPi = 6.2831853f;
constexpr float kMinVectorLengthSq = 1e-6f; // これより短い向きベクトルは、向きが定まらないとみなす
constexpr Vector3 kUp{0.0f, 1.0f, 0.0f};

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// レール上の距離 distance の位置から、水平に lateral(正は右)だけ離れた点の x, z を求める。
// 右は、レールの進行方向と真上の外積。進行方向が真上 / 真下のときは、+x を右とする
Vector3 PointBesideRail(const RailCameraComponent &rail, float totalLength, float distance, float lateral) {
  Vector3 railPosition;
  Vector3 railForward;
  rail.GetPointAndForward(distance / totalLength, railPosition, railForward);
  Vector3 right = Cross(kUp, railForward);
  if (Dot(right, right) < kMinVectorLengthSq) {
    right = {1.0f, 0.0f, 0.0f};
  }
  right = Normalize(right);
  return {railPosition.x + right.x * lateral, 0.0f, railPosition.z + right.z * lateral};
}

// 島を 1 つ作って、置き場所まで決める。モデルを読み込めなければ nullptr
std::unique_ptr<StageIsland> MakeIsland(const std::shared_ptr<ModelResource> &resource, const Vector3 &position, float scale, const Vector3 &rotate) {
  auto island = std::make_unique<StageIsland>();
  if (!island->Initialize(resource)) {
    return nullptr;
  }
  island->SetPlacement(position, scale, rotate);
  return island;
}
} // namespace

bool StageIslands::Initialize(const RailCameraComponent &rail, float cloudLayerTop) {
  entries_.clear();
  turretSlots_.clear();
  const float totalLength = rail.GetTotalLength();
  if (totalLength <= 0.0f) {
    return false;
  }
  auto *assets = AbsoluteEngine::AssetManager::GetInstance();

  // 飾りのモデルは 1 度だけ読み込んで共有する(AssetManager のキャッシュ)。読み込めた種類だけを使う
  std::vector<std::shared_ptr<ModelResource>> resources;
  for (const char *path : kDecorModelPaths) {
    if (auto resource = assets->Load<ModelResource>(path)) {
      resources.push_back(resource);
    }
  }
  const int32_t kindCount = static_cast<int32_t>(resources.size());

  // --- 飾りの島(固定の乱数。設定が同じなら毎回同じ配置。引く順番を変えると配置が変わる) ---
  if (kindCount > 0) {
    std::mt19937 rng(static_cast<uint32_t>(StageIslandDefaults::kSeed));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    int32_t index = 0;
    for (float distance = StageIslandDefaults::kFirstDistance; distance < totalLength;
         distance += StageIslandDefaults::kSpacing * (1.0f + (unit(rng) * 2.0f - 1.0f) * StageIslandDefaults::kSpacingJitter)) {
      const float scale = Lerp(StageIslandDefaults::kScaleMin, StageIslandDefaults::kScaleMax, unit(rng));
      // 左右は交互。大きい島ほど、上面の半径ぶん外へ離す(島の縁が、通り道へはみ出さないように)
      const float side = (index % 2 == 0) ? 1.0f : -1.0f;
      const float lateral = StageIslandDefaults::kCorridorHalfWidth + scale * StageIslandDefaults::kTopRadiusPerScale +
                            unit(rng) * StageIslandDefaults::kLateralRange;
      const float height = cloudLayerTop + Lerp(StageIslandDefaults::kHeightAboveCloudMin, StageIslandDefaults::kHeightAboveCloudMax, unit(rng));
      const float yaw = unit(rng) * kTwoPi;
      const float tiltX = (unit(rng) * 2.0f - 1.0f) * StageIslandDefaults::kMaxTiltRad;
      const float tiltZ = (unit(rng) * 2.0f - 1.0f) * StageIslandDefaults::kMaxTiltRad;
      const int32_t kind = static_cast<int32_t>(unit(rng) * static_cast<float>(kindCount)) % kindCount;
      ++index;

      // タレットの島の前後には置かない(乱数は引いたあとなので、ほかの島の配置は、これで変わらない)
      bool nearTurretIsland = false;
      for (const auto &spec : StageIslandDefaults::kTurretIslands) {
        nearTurretIsland |= std::fabs(distance - spec.distance) < StageIslandDefaults::kTurretClearDistance;
      }
      if (nearTurretIsland) continue;

      Vector3 position = PointBesideRail(rail, totalLength, distance, lateral * side);
      position.y = height; // 島は、レールの高さではなく、雲の上面を基準の高さにする
      if (auto island = MakeIsland(resources[kind], position, scale, {tiltX, yaw, tiltZ})) {
        entries_.push_back({std::move(island), distance});
      }
    }
  }

  // --- タレットの島(手で指定。傾けない。向きは、見た目の変化のために、位置から決める) ---
  if (auto turretResource = assets->Load<ModelResource>(kTurretModelPath)) {
    for (const auto &spec : StageIslandDefaults::kTurretIslands) {
      Vector3 position = PointBesideRail(rail, totalLength, spec.distance, spec.lateral);
      position.y = cloudLayerTop + StageIslandDefaults::kTurretIslandHeightAboveCloud;
      const float yaw = std::fmod(spec.distance, kTwoPi);
      if (auto island = MakeIsland(turretResource, position, spec.scale, {0.0f, yaw, 0.0f})) {
        entries_.push_back({std::move(island), spec.distance});
      }
    }
  }

  // 描画の絞り込みが、距離の昇順を前提にしている。並べ替えのあとで、タレットの島の一覧を作る(島の実体は動かない)
  std::sort(entries_.begin(), entries_.end(), [](const Entry &a, const Entry &b) { return a.distance < b.distance; });
  for (const auto &spec : StageIslandDefaults::kTurretIslands) {
    for (const Entry &entry : entries_) {
      // 同じ距離で、同じ島を探す(タレットの島の距離は、飾りの島と重ならない: 飾りは前後を空けている)
      if (entry.distance == spec.distance) {
        turretSlots_.push_back({entry.island.get(), entry.distance});
        break;
      }
    }
  }
  return !entries_.empty();
}

void StageIslands::Draw(Renderer *renderer, float railDistance) {
  for (const Entry &entry : entries_) {
    const float ahead = entry.distance - railDistance;
    if (ahead < -StageIslandDefaults::kDrawBehind) continue;
    if (ahead > StageIslandDefaults::kDrawAhead) break; // 距離の昇順なので、これより先はすべて遠い
    entry.island->Draw(renderer);
  }
}
