#include "BossShardInfo.h"

#include <filesystem>
#include <fstream>

#include "AbsoluteEngine/base/AssetPath.h"
#include "Logger.h"
#include "externals/nlohmann/json.hpp"

namespace {
// JSON の項目名
constexpr const char *kKeyShards = "shards";
constexpr const char *kKeyName = "name";
constexpr const char *kKeyKind = "kind";
constexpr const char *kKeyVolume = "volume";
constexpr const char *kKeyRadius = "radius";
} // namespace

bool BossShardInfo::Load(const std::string &path) {
  entries_.clear();

  const std::filesystem::path resolved = AbsoluteEngine::AssetPath::Resolve(path);
  std::ifstream file(resolved);
  if (!file) {
    Logger::GetInstance().Log("[BossShardInfo] file not found: %s", resolved.string().c_str());
    return false;
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const nlohmann::json::exception &e) {
    Logger::GetInstance().Log("[BossShardInfo] invalid json: %s (%s)", resolved.string().c_str(), e.what());
    return false;
  }

  const auto shardsIt = root.find(kKeyShards);
  if (shardsIt == root.end() || !shardsIt->is_array()) {
    Logger::GetInstance().Log("[BossShardInfo] \"%s\" must be an array: %s", kKeyShards, resolved.string().c_str());
    return false;
  }

  entries_.reserve(shardsIt->size());
  for (const nlohmann::json &item : *shardsIt) {
    // 名前のない欠片は、骨と対応づけられないので読み飛ばす
    if (!item.contains(kKeyName) || !item[kKeyName].is_string()) continue;
    BossShardEntry entry;
    entry.name = item[kKeyName].get<std::string>();
    entry.kind = item.value(kKeyKind, std::string{});
    entry.volume = item.value(kKeyVolume, 0.0f);
    entry.radius = item.value(kKeyRadius, 0.0f);
    entries_.push_back(std::move(entry));
  }
  return !entries_.empty();
}
