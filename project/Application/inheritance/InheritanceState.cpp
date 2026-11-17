#include "InheritanceState.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "AbsoluteEngine/base/AssetPath.h"
#include "../item/ItemCatalog.h"
#include "Logger.h"
#include "externals/nlohmann/json.hpp"

namespace {
// 保存先(exe 基準。StageProgress と同じ savedata フォルダ)
constexpr const char *kSaveDirectory = "savedata";
constexpr const char *kSaveFileName = "inheritance.json";
constexpr const char *kSaveTempSuffix = ".tmp"; // 書き込み途中で止まっても、保存ファイルが壊れないよう、一時ファイルに書いてから置き換える

// JSON の項目名
constexpr const char *kKeyGeneration = "generation";
constexpr const char *kKeyStacks = "stacks";
constexpr int kJsonIndent = 2;

std::filesystem::path GetSavePath() {
  return AbsoluteEngine::AssetPath::GetExeDirectory() / kSaveDirectory / kSaveFileName;
}
} // namespace

InheritanceState::InheritanceState() {
  Load_();
}

InheritanceState &InheritanceState::GetInstance() {
  static InheritanceState instance;
  return instance;
}

void InheritanceState::Load_() {
  generation_ = 0;
  inherited_.fill(0.0f);
  const std::filesystem::path path = GetSavePath();
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return; // 初めての起動(保存が無い)。継承なしで始める

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    Logger::GetInstance().Log("[Inheritance] cannot open the save file: %s", path.string().c_str());
    return;
  }
  // 壊れた保存ファイルで止まらないよう、例外を受けて、継承なしで始める
  try {
    const nlohmann::json root = nlohmann::json::parse(file);
    generation_ = (std::max)(0, root.value(kKeyGeneration, 0));
    const auto stacks = root.find(kKeyStacks);
    if (stacks != root.end() && stacks->is_object()) {
      for (size_t i = 0; i < kItemEffectKindCount; ++i) {
        const ItemEffectKind kind = static_cast<ItemEffectKind>(i);
        const char *name = ItemCatalog::GetEffectName(kind);
        const auto it = stacks->find(name);
        if (it != stacks->end() && it->is_number()) {
          inherited_[i] = (std::max)(0.0f, it->get<float>());
        }
      }
    }
  } catch (const std::exception &e) {
    Logger::GetInstance().Log("[Inheritance] the save file is broken (starting without inheritance): %s", e.what());
    generation_ = 0;
    inherited_.fill(0.0f);
  }
}

void InheritanceState::Save_() const {
  if (volatile_) return; // 仮の状態(展示)の間は、保存ファイルを書き込まない
  const std::filesystem::path path = GetSavePath();
  try {
    nlohmann::json stacks = nlohmann::json::object();
    for (size_t i = 0; i < kItemEffectKindCount; ++i) {
      stacks[ItemCatalog::GetEffectName(static_cast<ItemEffectKind>(i))] = inherited_[i];
    }
    const nlohmann::json root = {{kKeyGeneration, generation_}, {kKeyStacks, stacks}};

    std::filesystem::create_directories(path.parent_path());
    std::filesystem::path tempPath = path;
    tempPath += kSaveTempSuffix;
    {
      std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
      if (!file) {
        Logger::GetInstance().Log("[Inheritance] cannot write the save file: %s", tempPath.string().c_str());
        return;
      }
      file << root.dump(kJsonIndent);
    }
    std::filesystem::rename(tempPath, path); // 一時ファイルを、保存ファイルへ置き換える
  } catch (const std::exception &e) {
    Logger::GetInstance().Log("[Inheritance] failed to save (continuing): %s", e.what());
  }
}

void InheritanceState::Commit(const std::vector<Pick> &picks, float rarity, const ItemUpgradeSettings &settings) {
  for (const Pick &pick : picks) {
    float &stacks = inherited_[static_cast<size_t>(pick.kind)];
    const float added = static_cast<float>(pick.pickedCount) * rarity;
    // 上限(items.json の maxStacks)で頭打ち
    stacks = (std::min)(stacks + added, static_cast<float>(settings.Get(pick.kind).maxStacks));
  }
  ++generation_;
  Save_();
  Logger::GetInstance().Log("[Inheritance] committed: generation=%d picks=%zu rarity=%.2f", generation_, picks.size(), rarity);
}

void InheritanceState::Reset() {
  generation_ = 0;
  inherited_.fill(0.0f);
  Save_(); // 仮の状態の間は、Save_ が何もしない
  Logger::GetInstance().Log("[Inheritance] reset%s", volatile_ ? " (volatile)" : "");
}

void InheritanceState::BeginVolatile() {
  if (volatile_) return;
  backupGeneration_ = generation_;
  backupInherited_ = inherited_;
  generation_ = 0;
  inherited_.fill(0.0f);
  volatile_ = true;
  Logger::GetInstance().Log("[Inheritance] volatile mode begin (the save file is not written)");
}

void InheritanceState::EndVolatile() {
  if (!volatile_) return;
  generation_ = backupGeneration_;
  inherited_ = backupInherited_;
  volatile_ = false;
  Logger::GetInstance().Log("[Inheritance] volatile mode end (restored)");
}
