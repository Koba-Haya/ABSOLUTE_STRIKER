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
  // 読み込みは Load で明示的に行う(持ち主の GameData が、アプリの初期化で呼ぶ)。以前は Singleton で、最初の GetInstance のときに読んでいた
  void Load();

  // id に対応する定義を返す。無ければ nullptr
  const ItemDefinition *Find(const std::string &itemId) const;

  // 強化の種類 kind の名前(items.json の "effect" や、保存ファイルの項目名に使う文字。例: "maxHp")
  static const char *GetEffectName(ItemEffectKind kind);

  // すべてのアイテムの定義(JSON に書かれた順。開発用の表示などが使う)
  const std::vector<ItemDefinition> &GetItems() const { return items_; }

  const ItemPickupSettings &GetPickupSettings() const { return pickup_; }
  const ItemUpgradeSettings &GetUpgradeSettings() const { return upgrade_; }

private:
  std::vector<ItemDefinition> items_;
  ItemPickupSettings pickup_;
  ItemUpgradeSettings upgrade_;
};
