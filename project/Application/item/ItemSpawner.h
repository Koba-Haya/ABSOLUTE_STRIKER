#pragma once
#include "ItemDefinition.h"
#include "Type/Vector.h"
#include <functional>

class BaseScene;

// アイテムのオブジェクトを作って、シーンへ置くクラス(関数だけ)。
// 分けた理由: 「出現の手順」(モデル、テクスチャ、動きのコンポーネントを付けて置く)だけを担当し、
//             抽選(ItemDropTable)、取得の判定(ItemPickupComponent)、効果(ItemEffectApplier)を知らないため。
class ItemSpawner {
public:
  // position に、定義 item のアイテムを 1 個置く。onCollected は、自機が取った瞬間に 1 回呼ばれる
  static void Spawn(BaseScene &scene, const ItemDefinition &item, const ItemPickupSettings &settings, const Vector3 &position,
                    std::function<void()> onCollected);
};
