#include "BulletCatalog.h"

#include <fstream>
#include <set>

#include "AbsoluteEngine/base/AssetPath.h"
#include "externals/nlohmann/json.hpp"

namespace {
// 定義のファイル(exe 基準。Application/resources が resources/app へ複製される)
constexpr const char *kCatalogPath = "resources/app/bullet/bullets.json";

// JSON の項目名
constexpr const char *kKeyBullets = "bullets";
constexpr const char *kKeyId = "id";
constexpr const char *kKeyModel = "model";
constexpr const char *kKeyScale = "scale";
constexpr const char *kKeyRadius = "radius";
constexpr const char *kKeyColor = "color";
constexpr const char *kKeyCanBeShotDown = "canBeShotDown";

// 色の要素数(r, g, b, a)
constexpr size_t kColorComponentCount = 4;

[[noreturn]] void FailInvalid(const std::string &resolvedPath, const std::string &reason) {
  AbsoluteEngine::AssetPath::ReportMissingAndExit("bullet definition is invalid (" + reason + ")", resolvedPath);
}

// エラーの文に使う、要素の場所の表記(例: "bullets[2].scale")
std::string EntryLabel(size_t index, const char *key) {
  return std::string(kKeyBullets) + "[" + std::to_string(index) + "]." + key;
}

// 数値の項目を読む(省略したら既定値のまま)。数値でないときは、理由つきで終了する
void ReadNumber(const nlohmann::json &entry, const char *key, float &target, size_t index, const std::string &resolvedPath) {
  const auto it = entry.find(key);
  if (it == entry.end()) return;
  if (!it->is_number()) {
    FailInvalid(resolvedPath, EntryLabel(index, key) + " must be a number");
  }
  target = it->get<float>();
}

// 弾 1 つ分の定義を読む。不正な値は、理由つきで終了する
BulletDefinition ReadBullet(const nlohmann::json &entry, size_t index, const std::string &resolvedPath) {
  if (!entry.is_object()) {
    FailInvalid(resolvedPath, std::string(kKeyBullets) + "[" + std::to_string(index) + "] must be an object");
  }

  BulletDefinition def;

  // 識別子(必須)
  const auto idIt = entry.find(kKeyId);
  if (idIt == entry.end() || !idIt->is_string() || idIt->get<std::string>().empty()) {
    FailInvalid(resolvedPath, EntryLabel(index, kKeyId) + " must be a non-empty string");
  }
  def.id = idIt->get<std::string>();

  // モデル(省略可。省略したら、モデルを付けない弾)
  if (const auto it = entry.find(kKeyModel); it != entry.end()) {
    if (!it->is_string()) {
      FailInvalid(resolvedPath, EntryLabel(index, kKeyModel) + " must be a string");
    }
    def.modelPath = it->get<std::string>();
  }

  // 大きさ(どちらも 0 より大きいこと。0 以下だと、見えない、当たらない弾になる)
  ReadNumber(entry, kKeyScale, def.scale, index, resolvedPath);
  ReadNumber(entry, kKeyRadius, def.radius, index, resolvedPath);
  if (def.scale <= 0.0f) {
    FailInvalid(resolvedPath, EntryLabel(index, kKeyScale) + " must be positive");
  }
  if (def.radius <= 0.0f) {
    FailInvalid(resolvedPath, EntryLabel(index, kKeyRadius) + " must be positive");
  }

  // 色(省略可。書くなら、数値 4 つの配列)
  if (const auto it = entry.find(kKeyColor); it != entry.end()) {
    if (!it->is_array() || it->size() != kColorComponentCount) {
      FailInvalid(resolvedPath, EntryLabel(index, kKeyColor) + " must be an array of 4 numbers");
    }
    for (const auto &component : *it) {
      if (!component.is_number()) {
        FailInvalid(resolvedPath, EntryLabel(index, kKeyColor) + " must be an array of 4 numbers");
      }
    }
    def.color = Vector4{(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), (*it)[3].get<float>()};
  }

  // 撃ち落とせるか(省略可)
  if (const auto it = entry.find(kKeyCanBeShotDown); it != entry.end()) {
    if (!it->is_boolean()) {
      FailInvalid(resolvedPath, EntryLabel(index, kKeyCanBeShotDown) + " must be true or false");
    }
    def.canBeShotDown = it->get<bool>();
  }

  return def;
}
} // namespace

const BulletDefinition *BulletCatalog::Find(const std::string &bulletId) const {
  for (const auto &bullet : bullets_) {
    if (bullet.id == bulletId) return &bullet;
  }
  return nullptr;
}

void BulletCatalog::Load() {
  const std::filesystem::path path = AbsoluteEngine::AssetPath::Resolve(kCatalogPath);
  const std::string pathText = path.string();

  std::ifstream file(path);
  if (!file) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("bullet definition file", pathText);
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const nlohmann::json::exception &e) {
    FailInvalid(pathText, e.what());
  }

  const auto listIt = root.find(kKeyBullets);
  if (listIt == root.end() || !listIt->is_array() || listIt->empty()) {
    FailInvalid(pathText, std::string(kKeyBullets) + " must be a non-empty array");
  }

  bullets_.clear();
  std::set<std::string> seenIds; // 同じ id が 2 回書かれていないかを調べる(後の方が、黙って無視されるのを防ぐ)
  for (size_t i = 0; i < listIt->size(); ++i) {
    BulletDefinition def = ReadBullet((*listIt)[i], i, pathText);
    if (!seenIds.insert(def.id).second) {
      FailInvalid(pathText, "duplicate id \"" + def.id + "\"");
    }
    bullets_.push_back(std::move(def));
  }
}
