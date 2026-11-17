#include "RunEvaluator.h"

#include <algorithm>

namespace {
float Saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

// 割合(0 〜 1)と重みから、評価の 1 項目を作る
RatingEntry MakeEntry(float ratio, float weight) {
  RatingEntry entry;
  entry.ratio = Saturate(ratio);
  entry.points = entry.ratio * weight;
  return entry;
}

// 分母が 0 以下のときは、割合を決められないので、満点(1)として扱う(例: 敵が 1 体も出なかった)
float SafeRatio(float numerator, float denominator) {
  return denominator > 0.0f ? numerator / denominator : 1.0f;
}
} // namespace

RunResult RunEvaluator::Evaluate(const RunStats &stats, int score, int kills, int scoreTarget, int playerMaxHp,
                                 const RatingSettings &settings) {
  RunResult result;
  result.stats = stats;
  result.score = score;
  result.kills = kills;
  result.scoreTarget = scoreTarget;

  // スコア
  result.scoreEntry = MakeEntry(SafeRatio(static_cast<float>(score), static_cast<float>(scoreTarget)), settings.weightScore);

  // 撃破率(自滅した敵は、自機が倒せたかどうかに関わらないので、母数から外す)
  const int eligible = stats.enemiesSpawned - stats.enemiesSelfDestructed;
  result.killEntry = MakeEntry(SafeRatio(static_cast<float>(stats.enemiesKilled), static_cast<float>(eligible)), settings.weightKill);

  // 被ダメージの少なさ(ノーダメージが満点)
  // (最大 HP が 0 以下のときは、割合を決められないので、満点として扱う)
  const float damageRatio = playerMaxHp > 0 ? 1.0f - static_cast<float>(stats.damageTaken) / static_cast<float>(playerMaxHp) : 1.0f;
  result.damageEntry = MakeEntry(damageRatio, settings.weightDamage);

  // 編隊の全滅
  result.formationEntry = MakeEntry(SafeRatio(static_cast<float>(stats.formationsCleared), static_cast<float>(stats.formationsSpawned)),
                                    settings.weightFormation);

  // マルチロック(同時ロック数と、一斉撃破の、それぞれの目標に対する割合の平均)
  const float lockRatio = Saturate(SafeRatio(static_cast<float>(stats.maxLockCount), static_cast<float>(settings.lockTarget)));
  const float killsRatio = Saturate(SafeRatio(static_cast<float>(stats.maxKillsInWindow), static_cast<float>(settings.killsInWindowTarget)));
  result.multiLockEntry = MakeEntry((lockRatio + killsRatio) * 0.5f, settings.weightMultiLock);

  // ボス戦ノーダメージ
  result.bossNoDamageEntry = MakeEntry(stats.bossHitCount == 0 ? 1.0f : 0.0f, settings.weightBossNoDamage);

  result.totalPoints = result.scoreEntry.points + result.killEntry.points + result.damageEntry.points + result.formationEntry.points +
                       result.multiLockEntry.points + result.bossNoDamageEntry.points;
  result.rank = DecideRank(result.totalPoints, settings);
  return result;
}

RunRank RunEvaluator::DecideRank(float totalPoints, const RatingSettings &settings) {
  if (totalPoints >= settings.thresholdS) return RunRank::S;
  if (totalPoints >= settings.thresholdA) return RunRank::A;
  if (totalPoints >= settings.thresholdB) return RunRank::B;
  return RunRank::C;
}
