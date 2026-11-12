#pragma once
#include "ItemDropTable.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

class BaseScene;
namespace AbsoluteEngine {
class GameObject;
}
struct ItemDefinition;

// アイテムの仕組み全体の窓口。GameScene は、このクラスの 2 つの関数だけを呼ぶ。
//   BeginStage: ステージの開始(自機が決まった後)に 1 回
//   OnEnemyDestroyed: 敵が倒れたときに 1 回
// 分けた理由: シーンに処理を書かず、各クラス(抽選、出現、取得、効果、強化の状態)をつなぐ役を 1 か所にまとめるため。
//             落ちているアイテムの動き、取得判定は、各アイテムのコンポーネント(ItemPickupComponent)が行うので、毎フレームの更新は要らない。
class ItemSystem {
public:
  ItemSystem() = default;

  // ステージの開始で呼ぶ。強化の状態を(設定に従って)初期化し、持ち越した強化を自機へ反映する
  void BeginStage(const std::shared_ptr<AbsoluteEngine::GameObject> &player);

  // 敵 enemyName(オブジェクトの名前 = プレハブ名)が position で倒れた。抽選して、当たればアイテムを置く
  void OnEnemyDestroyed(BaseScene &scene, const std::string &enemyName, const Vector3 &position);

private:
  // 自機がアイテムを取った瞬間(効果を出す)
  void OnCollected_(const ItemDefinition &item);

  ItemDropTable dropTable_;
  std::weak_ptr<AbsoluteEngine::GameObject> player_;
};
