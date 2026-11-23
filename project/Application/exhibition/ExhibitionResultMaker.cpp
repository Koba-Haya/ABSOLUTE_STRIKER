#include "ExhibitionResultMaker.h"

#include <array>
#include <random>

#include "../result/RunEvaluator.h"
#include "Logger.h"

namespace {
// ランクごとの、成績の数字の見本(目標スコア 5000、最大 HP 5 の前提。重みは rating.json: スコア 25、撃破 20、被ダメージ 25、編隊 10、マルチロック 10、ボス戦ノーダメージ 10)
struct RankPreset {
  RunRank rank;
  float weight;       // 選ばれやすさ(大きいほど出やすい)
  float scoreRatio;   // スコアの、目標に対する割合
  RunStats stats;
};

constexpr RunStats MakeStats(int spawned, int killed, int formationsSpawned, int formationsCleared, int damage, int bossHits, int maxLock,
                             int maxKillsInWindow) {
  RunStats stats;
  stats.enemiesSpawned = spawned;
  stats.enemiesKilled = killed;
  stats.formationsSpawned = formationsSpawned;
  stats.formationsCleared = formationsCleared;
  stats.damageTaken = damage;
  stats.hitCount = damage;
  stats.bossHitCount = bossHits;
  stats.maxLockCount = maxLock;
  stats.maxKillsInWindow = maxKillsInWindow;
  stats.playSeconds = 150.0f;
  return stats;
}

// 見本の合計点(目安): S 約 98、A 約 83、B 約 57、C 約 27(境目は rating.json の S 90、A 75、B 55)
const std::array<RankPreset, 4> kPresets = {{
    {RunRank::S, 1.0f, 0.96f, MakeStats(40, 39, 5, 5, 0, 0, 6, 4)},
    {RunRank::A, 1.0f, 0.80f, MakeStats(40, 34, 5, 4, 1, 0, 5, 3)},
    {RunRank::B, 1.0f, 0.64f, MakeStats(40, 29, 5, 3, 2, 1, 4, 2)},
    {RunRank::C, 0.5f, 0.30f, MakeStats(40, 18, 5, 1, 4, 2, 3, 1)},
}};
} // namespace

RunResult ExhibitionResultMaker::Make(int scoreTarget, int playerMaxHp, const RatingSettings &settings) {
  // 直前と同じ見本を外して、重みで 1 つ選ぶ
  std::array<float, kPresets.size()> weights{};
  for (size_t i = 0; i < kPresets.size(); ++i) {
    weights[i] = static_cast<int>(i) == lastPreset_ ? 0.0f : kPresets[i].weight;
  }
  std::discrete_distribution<size_t> pick(weights.begin(), weights.end());
  const size_t index = pick(random_);
  lastPreset_ = static_cast<int>(index);

  const RankPreset &preset = kPresets[index];
  const int score = static_cast<int>(static_cast<float>(scoreTarget) * preset.scoreRatio);
  RunResult result = RunEvaluator::Evaluate(preset.stats, score, preset.stats.enemiesKilled, scoreTarget, playerMaxHp, settings);
  // 見本が、狙ったランクにならなかったとき(rating.json の境目を変えたときなど)は、ログに出す
  if (result.rank != preset.rank) {
    Logger::GetInstance().Log("[Exhibition] preset rank %s evaluated as %s (total=%.1f). Adjust kPresets or rating.json.",
                              RankToString(preset.rank), RankToString(result.rank), result.totalPoints);
  }
  Logger::GetInstance().Log("[Exhibition] result: rank=%s total=%.1f score=%d", RankToString(result.rank), result.totalPoints, result.score);
  return result;
}
