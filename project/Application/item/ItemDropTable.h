#pragma once
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct ItemDefinition;

// 「どの敵を倒したら、どのアイテムが、どの確率で出るか」を、resources/item/item_drops.json から読んで、抽選するクラス。
// 分けた理由: 出現の方針(確率、対象の敵)だけを担当し、アイテムの見た目(ItemCatalog)や、出現の処理(ItemSpawner)から
//             切り離すため。確率や対象の敵を変えるのは、JSON を直すだけで済む。
// 敵の種類は、敵のオブジェクトの名前(= プレハブの "name": Enemy、EnemyHoming、EnemyTurret、EnemyCrosser)で見分ける。
// 1 つの敵に複数の行を書いたときは、上から順に抽選し、最初に当たった 1 つだけが出る。
class ItemDropTable {
public:
  ItemDropTable();

  // 敵 enemyName を倒したときの抽選。当たったアイテムの定義を返す。外れ、対象外なら nullptr。
  // canDrop は「そのアイテムを今出す意味があるか」を返す関数(例: 上限に達したロックオン強化は出さない)。
  // false を返したアイテムは、当たっても出さない。
  template <typename CanDropFn>
  const ItemDefinition *Roll(const std::string &enemyName, CanDropFn canDrop) {
    const auto it = rules_.find(enemyName);
    if (it == rules_.end()) return nullptr;
    for (const DropRule &rule : it->second) {
      if (!canDrop(*rule.item)) continue;
      if (Next01_() < rule.chance) return rule.item;
    }
    return nullptr;
  }

private:
  // 抽選の 1 行
  struct DropRule {
    const ItemDefinition *item = nullptr; // ItemCatalog が持つ定義への参照(ItemCatalog は終了まで残る)
    float chance = 0.0f;                  // 出る確率(0 から 1)
  };

  void Load_();
  float Next01_();

  std::unordered_map<std::string, std::vector<DropRule>> rules_;
  std::mt19937 random_;
};
