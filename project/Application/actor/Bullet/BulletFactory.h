#pragma once
#include <memory>
#include "Type/Vector.h"

namespace AbsoluteEngine {
class GameObject;
class ISpawner;
}

// ホーミング弾（ベジェ曲線で追尾）の生成パラメータ
struct HomingBulletSpec {
  Vector3 startPos{ 0.0f, 0.0f, 0.0f };   // 始点（P0）
  Vector3 controlPos{ 0.0f, 0.0f, 0.0f }; // 制御点（P1）
  float duration = 0.6f;                  // 着弾までの時間（秒）
  float radius = 1.0f;                    // コライダー半径
  std::weak_ptr<AbsoluteEngine::GameObject> target; // 追尾対象
};

// ホーミング弾の GameObject(コライダー・追尾の動き・尾)を組み立てる。
// まっすぐ飛ぶ弾(自機の通常弾、敵弾、ボス弾)は、弾の定義の表(BulletCatalog)と BulletBuilder で組み立てる(使い回すため)。
// ホーミング弾は、撃つ数が少なく使い回す効果が小さいので、ここで毎回作る。
// 戻り値のオブジェクトは呼び出し側が ISpawner::Spawn() で登録すること。
namespace BulletFactory {

// spawner: 尾(RibbonTrailComponent)が、弾が消えるときに、尾をシーンへ残すために使う窓口。弾より長く生きること(シーンが持つ)
std::shared_ptr<AbsoluteEngine::GameObject> CreatePlayerHoming(const HomingBulletSpec& spec, AbsoluteEngine::ISpawner& spawner);

} // namespace BulletFactory
