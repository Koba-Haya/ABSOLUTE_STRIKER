#pragma once
#include "ItemDefinition.h"

class PlayerComponent;
class PlayerUpgradeState;

// アイテムの効果を、自機へ反映するクラス(状態を持たず、関数だけ)。
// 分けた理由: 「効果の種類ごとの処理」を 1 か所に集め、効果を足すとき(HP 回復など)に、
//             出現、取得、抽選のクラスを触らずに済むようにするため。
class ItemEffectApplier {
public:
  // このアイテムを今取る意味があるか(例: ロックオン上限が最大値なら false)。
  // false のアイテムは、敵を倒しても出さない(取っても何も起きないアイテムが落ちないようにする)
  static bool CanApply(const ItemDefinition &item, const PlayerUpgradeState &state, const ItemUpgradeSettings &settings);

  // 効果を、強化の状態と自機へ反映する。自機の見た目や UI へは触らない(HUD は、自機の値を毎フレーム読む)。
  // 効果が実際に出たか(上限で 0 になったら false)を返す
  static bool Apply(const ItemDefinition &item, PlayerUpgradeState &state, const ItemUpgradeSettings &settings,
                    PlayerComponent &player);

  // 強化の状態の合計を、自機へ反映し直す(ステージの開始で、持ち越した強化を戻すために呼ぶ)
  static void SyncToPlayer(const PlayerUpgradeState &state, const ItemUpgradeSettings &settings, PlayerComponent &player);
};
