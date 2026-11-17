#include "StageProgress.h"

#include <filesystem>
#include <fstream>

#include "AbsoluteEngine/base/AssetPath.h"
#include "Logger.h"
#include "externals/nlohmann/json.hpp"

namespace {
// 保存先(exe 基準)
constexpr const char *kSaveDirectory = "savedata";
constexpr const char *kSaveFileName = "progress.json";
constexpr const char *kSaveTempSuffix = ".tmp"; // 書き込み途中で止まっても、保存ファイルが壊れないよう、一時ファイルに書いてから置き換える

// JSON の項目名
constexpr const char *kKeyStages = "stages";
constexpr const char *kKeyUnlocked = "unlocked";
constexpr const char *kKeyUnlockShown = "unlockShown";
constexpr const char *kKeyBestScore = "bestScore";
constexpr const char *kKeyBestPoints = "bestPoints";
constexpr const char *kKeyBestRank = "bestRank";
// 点数の更新とみなす最小の差(浮動小数点の誤差で、同じ点数が「更新」にならないようにする)
constexpr float kPointsEpsilon = 0.01f;
constexpr int kJsonIndent = 2;

// 保存した文字("S" など)を、ランクにする。知らない文字のときは false
bool ParseRank(const std::string &text, RunRank &out) {
  for (RunRank rank : {RunRank::S, RunRank::A, RunRank::B, RunRank::C}) {
    if (text == RankToString(rank)) {
      out = rank;
      return true;
    }
  }
  return false;
}

std::filesystem::path GetSavePath() {
  return AbsoluteEngine::AssetPath::GetExeDirectory() / kSaveDirectory / kSaveFileName;
}
} // namespace

StageProgress::StageProgress() {
  Load_();
}

StageProgress &StageProgress::GetInstance() {
  static StageProgress instance;
  return instance;
}

void StageProgress::Load_() {
  entries_.clear();
  const std::filesystem::path path = GetSavePath();
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return; // 初めての起動(保存が無い)。空の進行で始める

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    Logger::GetInstance().Log("[StageProgress] cannot open the save file: %s", path.string().c_str());
    return;
  }
  // 壊れた保存ファイルで止まらないよう、例外を受けて、空の進行で始める
  try {
    const nlohmann::json root = nlohmann::json::parse(file);
    const auto stages = root.find(kKeyStages);
    if (stages == root.end() || !stages->is_object()) return;
    for (auto it = stages->begin(); it != stages->end(); ++it) {
      if (!it.value().is_object()) continue;
      Entry entry;
      entry.unlocked = it.value().value(kKeyUnlocked, false);
      entry.unlockShown = it.value().value(kKeyUnlockShown, false);
      // ベスト記録(3 つがそろっているときだけ、記録ありとして読む。壊れた値は、記録なしにする)
      const auto &value = it.value();
      if (value.contains(kKeyBestScore) && value.contains(kKeyBestPoints) && value.contains(kKeyBestRank) &&
          value[kKeyBestScore].is_number() && value[kKeyBestPoints].is_number() && value[kKeyBestRank].is_string()) {
        RunRank rank = RunRank::C;
        if (ParseRank(value[kKeyBestRank].get<std::string>(), rank)) {
          entry.best.exists = true;
          entry.best.score = value[kKeyBestScore].get<int>();
          entry.best.points = value[kKeyBestPoints].get<float>();
          entry.best.rank = rank;
        }
      }
      entries_[it.key()] = entry;
    }
  } catch (const std::exception &e) {
    Logger::GetInstance().Log("[StageProgress] the save file is broken (starting with an empty progress): %s", e.what());
    entries_.clear();
  }
}

void StageProgress::Save_() const {
  const std::filesystem::path path = GetSavePath();
  try {
    nlohmann::json stages = nlohmann::json::object();
    for (const auto &pair : entries_) {
      nlohmann::json stageJson = {{kKeyUnlocked, pair.second.unlocked}, {kKeyUnlockShown, pair.second.unlockShown}};
      if (pair.second.best.exists) {
        stageJson[kKeyBestScore] = pair.second.best.score;
        stageJson[kKeyBestPoints] = pair.second.best.points;
        stageJson[kKeyBestRank] = RankToString(pair.second.best.rank);
      }
      stages[pair.first] = stageJson;
    }
    const nlohmann::json root = {{kKeyStages, stages}};

    std::filesystem::create_directories(path.parent_path());
    std::filesystem::path tempPath = path;
    tempPath += kSaveTempSuffix;
    {
      std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
      if (!file) {
        Logger::GetInstance().Log("[StageProgress] cannot write the save file: %s", tempPath.string().c_str());
        return;
      }
      file << root.dump(kJsonIndent);
    }
    std::filesystem::rename(tempPath, path); // 一時ファイルを、保存ファイルへ置き換える
  } catch (const std::exception &e) {
    Logger::GetInstance().Log("[StageProgress] failed to save the progress (continuing): %s", e.what());
  }
}

bool StageProgress::IsUnlocked(const StageDefinition &stage) const {
  if (stage.unlocked) return true;
  const auto it = entries_.find(stage.id);
  return it != entries_.end() && it->second.unlocked;
}

void StageProgress::Unlock(const std::string &stageId) {
  Entry &entry = entries_[stageId];
  if (entry.unlocked) return; // すでに解放済み。演出の印を、また立てない
  entry.unlocked = true;
  entry.unlockShown = false;
  Save_();
}

std::string StageProgress::FindPendingUnlock() const {
  for (const auto &pair : entries_) {
    if (pair.second.unlocked && !pair.second.unlockShown) return pair.first;
  }
  return {};
}

BestUpdate StageProgress::RecordRun(const std::string &stageId, const RunResult &result) {
  Entry &entry = entries_[stageId];
  BestUpdate update;
  update.previous = entry.best;

  if (!entry.best.exists) {
    // 初めてのクリア: 今回の値が、そのままベストになる(ランクの比較は、できないので、ランクアップとは言わない)
    update.scoreImproved = true;
    update.pointsImproved = true;
    entry.best.exists = true;
    entry.best.score = result.score;
    entry.best.points = result.totalPoints;
    entry.best.rank = result.rank;
  } else {
    // スコア、点数、ランクを、それぞれ別に、最高の値へ更新する
    if (result.score > entry.best.score) {
      update.scoreImproved = true;
      entry.best.score = result.score;
    }
    if (result.totalPoints > entry.best.points + kPointsEpsilon) {
      update.pointsImproved = true;
      entry.best.points = result.totalPoints;
    }
    if (static_cast<int>(result.rank) > static_cast<int>(entry.best.rank)) {
      update.rankImproved = true;
      entry.best.rank = result.rank;
    }
  }

  if (update.scoreImproved || update.pointsImproved || update.rankImproved) {
    Save_();
  }
  return update;
}

void StageProgress::MarkUnlockShown(const std::string &stageId) {
  const auto it = entries_.find(stageId);
  if (it == entries_.end() || it->second.unlockShown) return;
  it->second.unlockShown = true;
  Save_();
}
