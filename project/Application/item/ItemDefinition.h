#pragma once
#include <array>
#include <cstddef>
#include <string>

#include "Type/Vector.h"

// アイテムまわりの「データだけ」の定義をまとめたヘッダ。
// 分けた理由: 設定の読み込み(ItemCatalog)、抽選(ItemDropTable)、出現(ItemSpawner)、
//             取得(ItemPickupComponent)、効果(ItemEffectApplier)が、共通で参照する型を 1 か所に置き、
//             各クラスが互いのヘッダに依存しないようにする(依存を一方向に保つ)ため。

// アイテムの効果(強化)の種類。強化を足すときは、ここに足して(kItemEffectKindCount の手前に)、
// ItemCatalog の名前の表と、ItemEffectApplier の処理を足す(出現、取得、抽選は変えなくてよい)
enum class ItemEffectKind {
  LockOnMax,     // ロックオンできる数の上限を増やす
  MaxHp,         // 最大 HP を増やす(増えた分、HP も回復する)
  LockRadius,    // ロックオンの範囲の半径を広げる
  DodgeCooldown, // 回避(バレルロール)のクールダウンを短くする
  FireRate,      // 通常弾の間隔を短くする(連射が速くなる)
  PullRadius,    // 落ちているアイテムの吸い寄せ範囲を広げる
  Count_,        // 種類の数(配列の大きさに使う。効果として使わない)
};
constexpr size_t kItemEffectKindCount = static_cast<size_t>(ItemEffectKind::Count_);

// アイテム 1 種類の定義(resources/item/items.json の "items" の 1 要素)
struct ItemDefinition {
  std::string id;           // 識別子(item_drops.json から参照する)
  std::string name;         // 取ったときに画面に出す文字(UTF-8。「最大HP +1」など)。省略したら id
  ItemEffectKind effect = ItemEffectKind::LockOnMax;
  int amount = 1;           // 効果の量(LockOnMax なら、上限を増やす数)
  std::string modelPath;    // モデル(glTF)。exe 基準のパス
  std::string texturePath;  // パレット画像。exe 基準のパス
  float scale = 1.0f;       // モデルの拡大率
  Vector3 color{1.0f, 1.0f, 1.0f}; // 色(パレット画像の色に掛ける。強化の種類を、色で見分けるため)
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

// 強化 1 種類の効き方(items.json の "upgrade.effects" の 1 要素)
struct ItemEffectSpec {
  float perStack = 1.0f; // 強化 1 回あたりの量(単位は、種類による。ロック上限なら +1、回避なら 秒、連射なら 間隔の割合)
  int maxStacks = 0;     // 強化できる回数の上限(0 なら、その強化は出ない)
};

// 強化まわりの設定(items.json の "upgrade")
struct ItemUpgradeSettings {
  // 種類ごとの効き方(ItemEffectKind の並びと対応)。省略した種類は、下の既定値のまま
  std::array<ItemEffectSpec, kItemEffectKindCount> effects = {{
      {1.0f, 3},   // LockOnMax: +1 を 3 回(3 -> 6)
      {1.0f, 3},   // MaxHp: +1 を 3 回(5 -> 8)
      {10.0f, 6},  // LockRadius: +10px を 6 回(75 -> 135)
      {0.1f, 5},   // DodgeCooldown: -0.1 秒を 5 回(0.8 -> 0.3)
      {0.05f, 5},  // FireRate: 間隔を 5% ずつ 5 回(-25%)
      {2.0f, 5},   // PullRadius: +2 を 5 回(+10)
  }};
  bool carryOverBetweenStages = false;    // ステージをまたいで強化を持ち越すか(今は、ステージ内だけ)

  const ItemEffectSpec &Get(ItemEffectKind kind) const { return effects[static_cast<size_t>(kind)]; }
};
