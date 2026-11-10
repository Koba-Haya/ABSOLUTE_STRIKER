#include "ItemCatalog.h"

#include <fstream>
#include <set>
#include <utility>

#include "AbsoluteEngine/base/AssetPath.h"
#include "Logger.h"
#include "externals/nlohmann/json.hpp"

namespace {
// 定義のファイル(exe 基準。resources フォルダごと出力先へ複製される)
constexpr const char *kCatalogPath = "resources/app/item/items.json";

// JSON の項目名
constexpr const char *kKeyPickup = "pickup";
constexpr const char *kKeyUpgrade = "upgrade";
constexpr const char *kKeyItems = "items";
constexpr const char *kKeyId = "id";
constexpr const char *kKeyEffect = "effect";
constexpr const char *kKeyAmount = "amount";
constexpr const char *kKeyModel = "model";
constexpr const char *kKeyTexture = "texture";
constexpr const char *kKeyScale = "scale";

// 効果の名前(items.json の "effect")と、コード側の ItemEffectKind の対応表。効果を増やすときは、ここに足す
constexpr std::pair<const char *, ItemEffectKind> kEffectNames[] = {
    {"lockOnMax", ItemEffectKind::LockOnMax},
};

[[noreturn]] void FailInvalid(const std::string &resolvedPath, const std::string &reason) {
  AbsoluteEngine::AssetPath::ReportMissingAndExit("item definition is invalid (" + reason + ")", resolvedPath);
}

bool ParseEffect(const std::string &name, ItemEffectKind &out) {
  for (const auto &entry : kEffectNames) {
    if (name == entry.first) {
      out = entry.second;
      return true;
    }
  }
  return false;
}

// 必須の文字列項目を読む。無い、文字列でない、空のときは、理由つきで終了する
std::string ReadRequiredString(const nlohmann::json &entry, const char *key, size_t index, const std::string &resolvedPath) {
  const auto it = entry.find(key);
  if (it == entry.end() || !it->is_string() || it->get<std::string>().empty()) {
    FailInvalid(resolvedPath, "items[" + std::to_string(index) + "]." + key + " must be a non-empty string");
  }
  return it->get<std::string>();
}

// 数値の項目を読む(省略したら既定値のまま)。数値でないときは、理由つきで終了する
template <typename T>
void ReadNumber(const nlohmann::json &section, const char *key, T &target, const std::string &resolvedPath) {
  const auto it = section.find(key);
  if (it == section.end()) return;
  if (!it->is_number()) {
    FailInvalid(resolvedPath, std::string(key) + " must be a number");
  }
  target = it->get<T>();
}
} // namespace

ItemCatalog::ItemCatalog() {
  Load_();
}

ItemCatalog &ItemCatalog::GetInstance() {
  static ItemCatalog instance;
  return instance;
}

const ItemDefinition *ItemCatalog::Find(const std::string &itemId) const {
  for (const auto &item : items_) {
    if (item.id == itemId) return &item;
  }
  return nullptr;
}

void ItemCatalog::Load_() {
  const std::filesystem::path path = AbsoluteEngine::AssetPath::Resolve(kCatalogPath);
  const std::string pathText = path.string();

  std::ifstream file(path);
  if (!file) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("item definition file", pathText);
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const nlohmann::json::exception &e) {
    FailInvalid(pathText, e.what());
  }

  // 動きの設定(省略した項目は、ItemPickupSettings の既定値のまま)
  if (const auto it = root.find(kKeyPickup); it != root.end() && it->is_object()) {
    ReadNumber(*it, "pullRadius", pickup_.pullRadius, pathText);
    ReadNumber(*it, "pickupRadius", pickup_.pickupRadius, pathText);
    ReadNumber(*it, "pullSpeed", pickup_.pullSpeed, pathText);
    ReadNumber(*it, "lifetimeSeconds", pickup_.lifetimeSeconds, pathText);
    ReadNumber(*it, "spinSpeed", pickup_.spinSpeed, pathText);
    ReadNumber(*it, "bobAmplitude", pickup_.bobAmplitude, pathText);
    ReadNumber(*it, "bobSpeed", pickup_.bobSpeed, pathText);
    ReadNumber(*it, "collectSeconds", pickup_.collectSeconds, pathText);
    ReadNumber(*it, "collectScaleUp", pickup_.collectScaleUp, pathText);
  }
  if (pickup_.pickupRadius > pickup_.pullRadius) {
    FailInvalid(pathText, "pickup.pickupRadius must not exceed pickup.pullRadius");
  }
  if (pickup_.collectSeconds <= 0.0f) {
    FailInvalid(pathText, "pickup.collectSeconds must be positive");
  }

  // 強化の設定
  if (const auto it = root.find(kKeyUpgrade); it != root.end() && it->is_object()) {
    ReadNumber(*it, "lockOnMaxCap", upgrade_.lockOnMaxCap, pathText);
    upgrade_.carryOverBetweenStages = it->value("carryOverBetweenStages", upgrade_.carryOverBetweenStages);
  }

  // アイテムの定義
  const auto itemsIt = root.find(kKeyItems);
  if (itemsIt == root.end() || !itemsIt->is_array() || itemsIt->empty()) {
    FailInvalid(pathText, std::string("\"") + kKeyItems + "\" must be a non-empty array");
  }
  std::set<std::string> usedIds; // ID の重複の検出用
  for (size_t i = 0; i < itemsIt->size(); ++i) {
    const nlohmann::json &entry = (*itemsIt)[i];
    if (!entry.is_object()) {
      FailInvalid(pathText, "items[" + std::to_string(i) + "] must be an object");
    }
    ItemDefinition def;
    def.id = ReadRequiredString(entry, kKeyId, i, pathText);
    const std::string effectName = ReadRequiredString(entry, kKeyEffect, i, pathText);
    if (!ParseEffect(effectName, def.effect)) {
      FailInvalid(pathText, "items[" + std::to_string(i) + "].effect is unknown: \"" + effectName + "\"");
    }
    def.modelPath = ReadRequiredString(entry, kKeyModel, i, pathText);
    def.texturePath = ReadRequiredString(entry, kKeyTexture, i, pathText);
    def.amount = entry.value(kKeyAmount, def.amount);
    def.scale = entry.value(kKeyScale, def.scale);
    if (!usedIds.insert(def.id).second) {
      FailInvalid(pathText, "duplicate item id: " + def.id);
    }
    items_.push_back(std::move(def));
  }

  Logger::GetInstance().Log("[Item] catalog loaded: %zu item(s) from %s", items_.size(), pathText.c_str());
}
