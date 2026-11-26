#pragma once
#include <array>

#include "ItemDefinition.h"

// 強化の合計(強化の種類ごとの「積み数」)を持つクラス。積み数は、2 つに分けて持つ。
//   継承した分(小数): 前の世代から引き継いだ強化(InheritanceState)。ランクの倍率を掛けてあるので、小数になる
//   今回取った分(整数): このステージで、アイテムを取って増えた強化。ステージが始まるたびに 0 へ戻る
// 合計 = 継承した分 + 今回取った分。強化の値は、合計(上限で頭打ち) x 1 回あたりの量(ItemEffectSpec::perStack)。
// 分けた理由: クリア後に、「今回取った分」から継承を選ぶので、継承した分と区別して持つ必要がある。
//             「継承するか」「持ち越すか」の方針は、ここに閉じ込める(効果を自機へ反映する ItemEffectApplier や、
//             アイテムの出現とは関係を持たない)。
// 持ち越しの仕組み: アプリが動いている間ずっと残る GameData が、1 個のインスタンスを持つ(以前は関数内 static の Persistent)。
//   ステージの開始(BeginStage)で、今回取った分を 0 へ戻す(items.json の carryOverBetweenStages が true なら、残す)。
//   継承した分は、ステージの開始のたびに、シーンが SetInherited で入れ直す。
//   (ゲームオーバーからのやり直しも、ステージの開始として扱う)
class PlayerUpgradeState {
public:
  // ステージの開始で呼ぶ。carryOver が false なら、今回取った分を 0 に戻す(継承した分は、変えない)
  void BeginStage(bool carryOver);

  // 継承した分(小数)を決める。ステージの開始で、InheritanceState の値を入れる
  void SetInherited(ItemEffectKind kind, float stacks) { inherited_[static_cast<size_t>(kind)] = stacks < 0.0f ? 0.0f : stacks; }

  float GetInherited(ItemEffectKind kind) const { return inherited_[static_cast<size_t>(kind)]; }
  // 今回取った分(整数)
  int GetEarned(ItemEffectKind kind) const { return earned_[static_cast<size_t>(kind)]; }
  // 合計(継承した分 + 今回取った分。上限での頭打ちは、しない。値を求める側が、頭打ちにする)
  float GetTotal(ItemEffectKind kind) const { return GetInherited(kind) + static_cast<float>(GetEarned(kind)); }

  // 今回取った分を amount だけ増やす。継承した分と合わせて、maxStacks を超えないようにする。実際に増えた量を返す。
  // (継承した分が小数のとき、取れる整数の数は、上限から継承した分を引いて、切り捨てた数)
  int AddEarned(ItemEffectKind kind, int amount, int maxStacks);

  // あと何回、取れるか(0 なら、取っても何も起きない)
  int GetRemainingEarnable(ItemEffectKind kind, int maxStacks) const;

private:
  std::array<float, kItemEffectKindCount> inherited_{};
  std::array<int, kItemEffectKindCount> earned_{};
};
