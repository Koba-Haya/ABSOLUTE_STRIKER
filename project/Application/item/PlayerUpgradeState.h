#pragma once
#include <array>

#include "ItemDefinition.h"

// 取得した強化の合計(強化の種類ごとの「積み数」)を持つクラス。
// 分けた理由: 「強化がどれだけたまっているか」と「ステージをまたいで持ち越すか」の方針を、ここ 1 か所に閉じ込めるため。
//             持ち越しの有無は、items.json の upgrade.carryOverBetweenStages の 1 つの値で切り替える。
//             効果をどう自機へ反映するか(ItemEffectApplier)や、アイテムの出現とは関係を持たない。
// 積み数: 強化 1 回につき 1 つ(アイテムの amount 個)ずつ増える。強化の値は、積み数 x 1 回あたりの量(ItemEffectSpec::perStack)。
// 持ち越しの仕組み: 強化の値は、アプリが動いている間ずっと残る 1 個のインスタンス(Persistent)に持つ。
//   ステージの開始(BeginStage)で、持ち越さない設定なら 0 に戻し、持ち越す設定なら、前のステージの値をそのまま使う。
//   (ゲームオーバーからのやり直しも、ステージの開始として扱う)
class PlayerUpgradeState {
public:
  // アプリの間ずっと残る、強化の状態
  static PlayerUpgradeState &Persistent();

  // ステージの開始で呼ぶ。carryOver が false なら、強化をすべて 0 に戻す
  void BeginStage(bool carryOver);

  // 強化 kind の積み数
  int GetStacks(ItemEffectKind kind) const { return stacks_[static_cast<size_t>(kind)]; }

  // 強化 kind の積み数を amount だけ増やす。合計が maxStacks を超えないようにする。実際に増えた量を返す
  int AddStacks(ItemEffectKind kind, int amount, int maxStacks);

private:
  PlayerUpgradeState() = default;

  std::array<int, kItemEffectKindCount> stacks_{};
};
