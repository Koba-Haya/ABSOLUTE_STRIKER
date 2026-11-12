#pragma once
#include <string>

// アイテムまわりの「データだけ」の定義をまとめたヘッダ。
// 分けた理由: 設定の読み込み(ItemCatalog)、抽選(ItemDropTable)、出現(ItemSpawner)、
//             取得(ItemPickupComponent)、効果(ItemEffectApplier)が、共通で参照する型を 1 か所に置き、
//             各クラスが互いのヘッダに依存しないようにする(依存を一方向に保つ)ため。

// アイテムの効果の種類。HP 回復などを足すときは、ここに足して、
// ItemCatalog の名前の表と、ItemEffectApplier の処理を足す(出現、取得、抽選は変えなくてよい)
enum class ItemEffectKind {
  LockOnMax, // ロックオンできる数の上限を増やす
};

// アイテム 1 種類の定義(resources/item/items.json の "items" の 1 要素)
struct ItemDefinition {
  std::string id;           // 識別子(item_drops.json から参照する)
  ItemEffectKind effect = ItemEffectKind::LockOnMax;
  int amount = 1;           // 効果の量(LockOnMax なら、上限を増やす数)
  std::string modelPath;    // モデル(glTF)。exe 基準のパス
  std::string texturePath;  // パレット画像。exe 基準のパス
  float scale = 1.0f;       // モデルの拡大率
};

// 落ちているアイテムの動きの設定(全アイテム共通。items.json の "pickup")
struct ItemPickupSettings {
  float pullRadius = 10.0f;          // この距離以内に自機が入ると、自機へ吸い寄せられる
  float pickupRadius = 2.2f;         // この距離以内で取得する
  float pullSpeed = 28.0f;           // 吸い寄せの速さ(ユニット/秒)。レールの前進より十分速くする
  float lifetimeSeconds = 25.0f;     // 取られなかったアイテムが消えるまでの時間(秒)
  float spinSpeed = 2.5f;            // 自転の速さ(ラジアン/秒)
  float bobAmplitude = 0.25f;        // 上下のゆれの大きさ
  float bobSpeed = 3.0f;             // 上下のゆれの速さ(ラジアン/秒)
  float collectSeconds = 0.25f;      // 取得の演出(縮みながら自機へ吸い込まれる)の長さ(秒)
  float collectScaleUp = 1.4f;       // 取得の演出で、最初に少し膨らむ倍率
};

// 強化まわりの設定(items.json の "upgrade")
struct ItemUpgradeSettings {
  int lockOnMaxCap = 6;                   // ロックオン上限の最大値(これ以上は増えない)
  bool carryOverBetweenStages = false;    // ステージをまたいで強化を持ち越すか(今は、ステージ内だけ)
};
