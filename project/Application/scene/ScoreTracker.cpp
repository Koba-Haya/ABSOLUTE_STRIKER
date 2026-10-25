#include "ScoreTracker.h"

void ScoreTracker::RegisterFormationMember(int formationId) {
  ++formationRemaining_[formationId];
  ++formationTotal_[formationId];
}

void ScoreTracker::OnEnemyKilled() {
  // 編隊メンバーかどうかに関わらず、1体倒すごとに基礎点を加算する。
  // 編隊全滅時のボーナスは OnFormationMemberKilled 側でこれとは別に上乗せする。
  score_ += kScorePerKill;
  ++killCount_;
}

int ScoreTracker::OnFormationMemberKilled(int formationId) {
  auto it = formationRemaining_.find(formationId);
  if (it == formationRemaining_.end()) return 0;

  if (--it->second > 0) return 0;

  const auto totalIt = formationTotal_.find(formationId);
  const int total = (totalIt != formationTotal_.end()) ? totalIt->second : 0;

  formationRemaining_.erase(it);
  formationTotal_.erase(formationId);

  // 編隊メンバーは撃破のたびに既に kScorePerKill ずつ加算済みなので、
  // ここでは「倍率を適用した場合との差分」だけを追加ボーナスとして上乗せする。
  // （合計は total * kScorePerKill * kFormationBonusMultiplier になる）
  score_ += kScorePerKill * total * (kFormationBonusMultiplier - 1);
  return total;
}
