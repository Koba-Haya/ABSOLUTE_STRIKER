#pragma once
#include <array>
#include <vector>

#include "../item/ItemDefinition.h"

// 継承(前の世代から引き継いだ強化と、世代数)を持って、保存するクラス。
//
// 分けた理由: 「ステージの進行(解放、ベスト記録)」(StageProgress)と、「継承」は、別の責務で、リセットの単位も違う
// (どちらも「はじめから」で消えるが、役目が別)。保存のファイルも分ける(savedata/inheritance.json)。
// 持ち方: 世代数と、強化の種類ごとの「継承した積み数」(小数。ランクの倍率を掛けてあるため)。
// 上限は、items.json の maxStacks で頭打ちにする。
// 使い方: クリア画面で、選んだ強化を Commit する。ステージの開始で、GetInheritedStacks を、PlayerUpgradeState へ入れる。
//   「はじめから」で Reset。
// 保存: exe の横の savedata/inheritance.json(失敗しても、ゲームは続ける。ログに出す)。
//   形式: {"generation": 2, "stacks": {"lockOnMax": 1.0, "maxHp": 3.0, ...}}

class InheritanceState {
public:
  // 保存ファイルを読む(持ち主の GameData が、アプリの初期化で呼ぶ)。無い、壊れているときは、継承なしで始める。
  // 以前は Singleton で、最初の GetInstance のときに読んでいた
  void Load();

  // 世代数(最初のプレイは 0 世代。クリアして継承を確定するたびに、1 ずつ進む)
  int GetGeneration() const { return generation_; }

  // 強化 kind の、継承した積み数(小数)
  float GetInheritedStacks(ItemEffectKind kind) const { return inherited_[static_cast<size_t>(kind)]; }

  // 継承する強化(種類と、その種類を何個継承するか。取った 1 個ごとに 1 枠を使うので、個数は、使う枠の数と同じ)
  struct Pick {
    ItemEffectKind kind = ItemEffectKind::LockOnMax;
    int pickedCount = 0;
  };

  // 継承を確定する。picks の各強化について、「選んだ個数 x rarity」を、継承した積み数へ足す(上限で頭打ち)。
  // 世代数を 1 進めて、保存する。picks が空でも、世代は進む
  void Commit(const std::vector<Pick> &picks, float rarity, const ItemUpgradeSettings &settings);

  // 継承と世代数を、すべて消して(最初の状態へ戻して)保存する(「はじめから」)。仮の状態の間は、保存しない
  void Reset();

  // 仮の状態に入る(展示モード用)。いまの継承と世代数を退避して、最初の状態から始める。仮の状態の間は、保存ファイルを書き込まない。
  // すでに仮の状態のときは、何もしない
  void BeginVolatile();
  // 仮の状態を終えて、退避した継承と世代数へ戻す(保存ファイルは、そのまま)。仮の状態でないときは、何もしない
  void EndVolatile();

private:
  // 保存する。失敗したときは、ログに出して、続ける
  void Save_() const;

  int generation_ = 0;
  std::array<float, kItemEffectKindCount> inherited_{};
  bool volatile_ = false;                                 // 仮の状態か(true の間は、保存しない)
  int backupGeneration_ = 0;                              // 仮の状態に入る前の世代数
  std::array<float, kItemEffectKindCount> backupInherited_{}; // 仮の状態に入る前の継承
};
