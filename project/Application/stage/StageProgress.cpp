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
constexpr int kJsonIndent = 2;

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
      stages[pair.first] = {{kKeyUnlocked, pair.second.unlocked}, {kKeyUnlockShown, pair.second.unlockShown}};
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

void StageProgress::MarkUnlockShown(const std::string &stageId) {
  const auto it = entries_.find(stageId);
  if (it == entries_.end() || it->second.unlockShown) return;
  it->second.unlockShown = true;
  Save_();
}
