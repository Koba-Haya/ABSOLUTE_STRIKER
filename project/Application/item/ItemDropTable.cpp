#include "ItemDropTable.h"

#include <fstream>

#include "AbsoluteEngine/base/AssetPath.h"
#include "ItemCatalog.h"
#include "Logger.h"
#include "externals/nlohmann/json.hpp"

namespace {
// 抽選表のファイル(exe 基準)
constexpr const char *kDropTablePath = "resources/app/item/item_drops.json";

// JSON の項目名
constexpr const char *kKeyDrops = "drops";
constexpr const char *kKeyEnemy = "enemy";
constexpr const char *kKeyItem = "item";
constexpr const char *kKeyChance = "chance";

[[noreturn]] void FailInvalid(const std::string &resolvedPath, const std::string &reason) {
  AbsoluteEngine::AssetPath::ReportMissingAndExit("item drop table is invalid (" + reason + ")", resolvedPath);
}
} // namespace

ItemDropTable::ItemDropTable() : random_(std::random_device{}()) {
  Load_();
}

float ItemDropTable::Next01_() {
  return std::uniform_real_distribution<float>(0.0f, 1.0f)(random_);
}

void ItemDropTable::Load_() {
  const std::filesystem::path path = AbsoluteEngine::AssetPath::Resolve(kDropTablePath);
  const std::string pathText = path.string();

  std::ifstream file(path);
  if (!file) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("item drop table file", pathText);
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const nlohmann::json::exception &e) {
    FailInvalid(pathText, e.what());
  }

  const auto dropsIt = root.find(kKeyDrops);
  if (dropsIt == root.end() || !dropsIt->is_array()) {
    FailInvalid(pathText, std::string("\"") + kKeyDrops + "\" must be an array");
  }

  const ItemCatalog &catalog = ItemCatalog::GetInstance();
  for (size_t i = 0; i < dropsIt->size(); ++i) {
    const nlohmann::json &entry = (*dropsIt)[i];
    const std::string where = "drops[" + std::to_string(i) + "]";
    if (!entry.is_object() || !entry.contains(kKeyEnemy) || !entry[kKeyEnemy].is_string() || !entry.contains(kKeyItem) ||
        !entry[kKeyItem].is_string() || !entry.contains(kKeyChance) || !entry[kKeyChance].is_number()) {
      FailInvalid(pathText, where + " needs \"enemy\"(string), \"item\"(string), \"chance\"(number)");
    }

    DropRule rule;
    const std::string itemId = entry[kKeyItem].get<std::string>();
    rule.item = catalog.Find(itemId);
    if (!rule.item) {
      FailInvalid(pathText, where + ".item is not defined in items.json: \"" + itemId + "\"");
    }
    rule.chance = entry[kKeyChance].get<float>();
    if (rule.chance < 0.0f || rule.chance > 1.0f) {
      FailInvalid(pathText, where + ".chance must be between 0 and 1");
    }
    rules_[entry[kKeyEnemy].get<std::string>()].push_back(rule);
  }

  Logger::GetInstance().Log("[Item] drop table loaded: %zu rule(s) from %s", dropsIt->size(), pathText.c_str());
}
