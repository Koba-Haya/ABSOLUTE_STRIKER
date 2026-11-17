#pragma once
#include <deque>
#include <unordered_set>

#include "RunStats.h"

// 1 回のプレイ(ステージ 1 本)の、成績の元になる数字を数えるクラス。リザルト画面の評価(RunEvaluator)が、この数字を読む。
//
// 分けた理由: 「何を、どれだけ数えたか」(計測)と、「その数字を、どう点数や評価にするか」(評価)は別の責務。
// 計測だけをここに閉じ込めて、評価の基準(しきい値、重み)を変えても、計測のコードを触らなくてよいようにする。
// スコアと撃破数は、ゲーム中の HUD が使う ScoreTracker にあるので、ここでは持たない(二重に数えない)。
//
// 数える数字:
//   出現した敵、自機が倒した敵、自滅した敵(体当たりなど。自機が倒したのではないので、撃破率の母数から外す)
//   出現した編隊、全滅させた編隊
//   被ダメージ(HP の減った量)、被弾の回数、ボス戦の間の被弾の回数
//   プレイ時間(ゲームの時間で、通常の進行とボス戦の間だけ。ステージ開始の演出と、撃破の演出は、含めない)
//   同時ロック数の最大、短い時間の窓の中での撃破数の最大(一斉撃破の目安)
// 使い方: 敵の出現と撃破のたびに OnXxx を呼び、毎フレーム Update を呼ぶ。数字は GetStats で、まとめて受け取る。

namespace RunStatsDefaults {
constexpr float kMultiKillWindowSeconds = 0.6f; // 一斉撃破とみなす、撃破の間隔の窓(秒。ゲームの時間)
constexpr int kNoHp = -1;                       // 前のフレームの HP が、まだ分からない印
} // namespace RunStatsDefaults

class RunStatsTracker {
public:
  // 数字をすべて 0 に戻す(ステージの開始時)
  void Reset();

  // 敵が出現したときに呼ぶ。編隊のメンバーなら、inFormation を true にして、編隊の ID を渡す
  void OnEnemySpawned(bool inFormation, int formationId);

  // 自機が敵を 1 体倒したときに呼ぶ
  void OnEnemyKilled();

  // 敵が自滅したとき(自機が倒したのではない)に呼ぶ
  void OnEnemySelfDestructed();

  // 編隊を全部倒し切ったときに呼ぶ
  void OnFormationCleared();

  // 毎フレーム呼ぶ。
  // deltaTime: ゲームの時間の 1 フレーム(秒)
  // counting: 数えてよいときだけ true(通常の進行とボス戦の間。ステージ開始の演出や、撃破・死亡の演出の間は false)
  // isBossPhase: ボス戦の間か
  // playerHp: 自機のいまの HP(前のフレームより減っていれば、被ダメージとして数える)
  // lockedCount: 自機のいまの同時ロック数
  void Update(float deltaTime, bool counting, bool isBossPhase, int playerHp, int lockedCount);

  // 数字の一式を返す
  const RunStats &GetStats() const { return stats_; }

private:
  RunStats stats_;
  std::unordered_set<int> formationIds_; // 出現した編隊の ID(編隊の数を、メンバーの数ではなく、種類で数えるため)
  std::deque<float> recentKillTimes_;    // 最近の撃破の時刻(プレイ時間。一斉撃破の窓の判定に使う)
  int lastHp_ = RunStatsDefaults::kNoHp; // 前のフレームの HP
};
