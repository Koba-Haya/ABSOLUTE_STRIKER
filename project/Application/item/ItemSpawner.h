#pragma once
#include "ItemDefinition.h"
#include "Type/Vector.h"
#include <functional>

namespace AbsoluteEngine {
class ISceneQuery;
class ISpawner;
}

// アイテムのオブジェクトを作って、シーンへ置くクラス(関数だけ)。
// 分けた理由: 「出現の手順」(モデル、テクスチャ、動きのコンポーネントを付けて置く)だけを担当し、
//             抽選(ItemDropTable)、取得の判定(ItemPickupComponent)、効果(ItemEffectApplier)を知らないため。
class PlayerUpgradeState;

class ItemSpawner {
public:
  // position に、定義 item のアイテムを 1 個置く。onCollected は、自機が取った瞬間に 1 回呼ばれる
  // spawner: アイテムを置く窓口。query: 出したアイテムが、自機を探すために持つ窓口。
  // upgradeState: 自機の強化の状態(吸い寄せ範囲の強化を読む。所有しない。アイテムより長く生きること)
  static void Spawn(AbsoluteEngine::ISpawner &spawner, AbsoluteEngine::ISceneQuery &query, const ItemDefinition &item, const ItemPickupSettings &settings, const ItemUpgradeSettings &upgrade, const PlayerUpgradeState &upgradeState, const Vector3 &position,
                    std::function<void()> onCollected);
};
