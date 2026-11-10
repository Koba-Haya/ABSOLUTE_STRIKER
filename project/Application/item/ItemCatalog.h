#pragma once
#include "ItemDefinition.h"
#include <string>
#include <vector>

// アイテムの定義と設定を、resources/item/items.json から読み込んで持つクラス。
// 分けた理由: 「何があるか」(データ)の読み込みと検証だけを担当し、出現や取得の処理を持たないため。
//             アイテムの種類を足すときは、JSON に書くだけでよい(効果の種類を増やす場合のみ ItemEffectKind を足す)。
// 読み込みに失敗したとき(ファイルが無い、項目が不正)は、原因を表示して終了する(StageCatalog と同じ方針。
// 黙って進むと、アイテムが出ない原因が分からなくなるため)。
class ItemCatalog {
public:
  static ItemCatalog &GetInstance();

  // id に対応する定義を返す。無ければ nullptr
  const ItemDefinition *Find(const std::string &itemId) const;

  const ItemPickupSettings &GetPickupSettings() const { return pickup_; }
  const ItemUpgradeSettings &GetUpgradeSettings() const { return upgrade_; }

private:
  ItemCatalog();
  void Load_();

  std::vector<ItemDefinition> items_;
  ItemPickupSettings pickup_;
  ItemUpgradeSettings upgrade_;
};
