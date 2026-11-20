#include "RunStatsTracker.h"

#include <algorithm>

void RunStatsTracker::Reset() {
  stats_ = RunStats{};
  formationIds_.clear();
  recentKillTimes_.clear();
  lastHp_ = RunStatsDefaults::kNoHp;
}

void RunStatsTracker::OnEnemySpawned(bool inFormation, int formationId) {
  ++stats_.enemiesSpawned;
  if (inFormation && formationIds_.insert(formationId).second) {
    ++stats_.formationsSpawned; // その編隊の、最初のメンバーが出現したときに、編隊 1 つと数える
  }
}

void RunStatsTracker::OnEnemyKilled() {
  ++stats_.enemiesKilled;

  // 一斉撃破: 窓より古い撃破を捨てて、窓の中の撃破数の最大を更新する(窓の幅は、プレイ時間で測る)
  const float now = stats_.playSeconds;
  recentKillTimes_.push_back(now);
  while (!recentKillTimes_.empty() && now - recentKillTimes_.front() > RunStatsDefaults::kMultiKillWindowSeconds) {
    recentKillTimes_.pop_front();
  }
  stats_.maxKillsInWindow = (std::max)(stats_.maxKillsInWindow, static_cast<int>(recentKillTimes_.size()));
}

void RunStatsTracker::OnEnemySelfDestructed() {
  ++stats_.enemiesSelfDestructed;
}

void RunStatsTracker::OnFormationCleared() {
  ++stats_.formationsCleared;
}

void RunStatsTracker::Update(float deltaTime, bool counting, bool isBossPhase, int playerHp, int lockedCount) {
  // 数えないとき(ステージ開始の演出、撃破・死亡の演出の間)は、HP の記録だけを更新する(死亡で HP が 0 になる分を、被ダメージにしないため)
  if (counting) {
    stats_.playSeconds += deltaTime;
    stats_.maxLockCount = (std::max)(stats_.maxLockCount, lockedCount);
    // HP が前のフレームより減っていれば、被ダメージ(回復や蘇生で増えた分は、数えない)
    if (lastHp_ != RunStatsDefaults::kNoHp && playerHp < lastHp_) {
      stats_.damageTaken += lastHp_ - playerHp;
      ++stats_.hitCount;
      if (isBossPhase) ++stats_.bossHitCount;
    }
  }
  lastHp_ = playerHp;
}
