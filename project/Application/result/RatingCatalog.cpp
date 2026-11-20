#include "RatingCatalog.h"

#include <fstream>
#include <string>

#include "AbsoluteEngine/base/AssetPath.h"
#include "externals/nlohmann/json.hpp"

namespace {
// 評価の基準のファイル(exe 基準。resources フォルダごと出力先へ複製される)
constexpr const char *kRatingPath = "resources/app/stage/rating.json";

// JSON の項目名
constexpr const char *kKeyWeights = "weights";
constexpr const char *kKeyScore = "score";
constexpr const char *kKeyKill = "kill";
constexpr const char *kKeyDamage = "damage";
constexpr const char *kKeyFormation = "formation";
constexpr const char *kKeyMultiLock = "multiLock";
constexpr const char *kKeyBossNoDamage = "bossNoDamage";
constexpr const char *kKeyTargets = "multiLockTargets";
constexpr const char *kKeyLockTarget = "lock";
constexpr const char *kKeyKillsInWindowTarget = "killsInWindow";
constexpr const char *kKeyRanks = "ranks";
constexpr const char *kKeyRankS = "S";
constexpr const char *kKeyRankA = "A";
constexpr const char *kKeyRankB = "B";
constexpr const char *kKeyInheritance = "inheritance";
constexpr const char *kKeySlots = "slots";
constexpr const char *kKeyRarity = "rarity";

// 定義が正しくないときの終了。何が悪いかを、探したファイルの場所と一緒に表示する
[[noreturn]] void FailInvalid(const std::string &resolvedPath, const std::string &reason) {
  AbsoluteEngine::AssetPath::ReportMissingAndExit("rating settings are invalid (" + reason + ")", resolvedPath);
}

// 必須の項目(オブジェクト)を取り出す。無い、オブジェクトでないときは、理由つきで終了する
const nlohmann::json &RequireObject(const nlohmann::json &parent, const char *key, const std::string &resolvedPath) {
  const auto it = parent.find(key);
  if (it == parent.end() || !it->is_object()) {
    FailInvalid(resolvedPath, std::string(key) + " must be an object");
  }
  return *it;
}

// 必須の数値を取り出す。無い、数値でないときは、理由つきで終了する
float RequireNumber(const nlohmann::json &parent, const char *key, const std::string &resolvedPath) {
  const auto it = parent.find(key);
  if (it == parent.end() || !it->is_number()) {
    FailInvalid(resolvedPath, std::string(key) + " must be a number");
  }
  return it->get<float>();
}
} // namespace

RatingCatalog::RatingCatalog() {
  Load_();
}

RatingCatalog &RatingCatalog::GetInstance() {
  static RatingCatalog instance;
  return instance;
}

void RatingCatalog::Load_() {
  const std::string path = AbsoluteEngine::AssetPath::Resolve(kRatingPath).string();
  std::ifstream file(AbsoluteEngine::AssetPath::Resolve(kRatingPath), std::ios::binary);
  if (!file) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("rating settings file", path);
  }
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const std::exception &e) {
    FailInvalid(path, std::string("parse error: ") + e.what());
  }

  const nlohmann::json &weights = RequireObject(root, kKeyWeights, path);
  settings_.weightScore = RequireNumber(weights, kKeyScore, path);
  settings_.weightKill = RequireNumber(weights, kKeyKill, path);
  settings_.weightDamage = RequireNumber(weights, kKeyDamage, path);
  settings_.weightFormation = RequireNumber(weights, kKeyFormation, path);
  settings_.weightMultiLock = RequireNumber(weights, kKeyMultiLock, path);
  settings_.weightBossNoDamage = RequireNumber(weights, kKeyBossNoDamage, path);

  const nlohmann::json &targets = RequireObject(root, kKeyTargets, path);
  settings_.lockTarget = static_cast<int>(RequireNumber(targets, kKeyLockTarget, path));
  settings_.killsInWindowTarget = static_cast<int>(RequireNumber(targets, kKeyKillsInWindowTarget, path));
  // 目標が 0 以下だと、割り算が壊れる
  if (settings_.lockTarget <= 0 || settings_.killsInWindowTarget <= 0) {
    FailInvalid(path, "multiLockTargets must be positive");
  }

  const nlohmann::json &ranks = RequireObject(root, kKeyRanks, path);
  settings_.thresholdS = RequireNumber(ranks, kKeyRankS, path);
  settings_.thresholdA = RequireNumber(ranks, kKeyRankA, path);
  settings_.thresholdB = RequireNumber(ranks, kKeyRankB, path);
  // しきい値は、S > A > B の順(逆だと、ランクが決まらない)
  if (!(settings_.thresholdS > settings_.thresholdA && settings_.thresholdA > settings_.thresholdB)) {
    FailInvalid(path, "ranks must satisfy S > A > B");
  }

  // 継承のルール(ランクごとの、枠数と倍率)。書くときは、4 つのランクすべてを書く
  const nlohmann::json &inheritance = RequireObject(root, kKeyInheritance, path);
  const std::pair<const char *, RunRank> rankKeys[] = {
      {kKeyRankS, RunRank::S}, {kKeyRankA, RunRank::A}, {kKeyRankB, RunRank::B}, {"C", RunRank::C}};
  for (const auto &entry : rankKeys) {
    const nlohmann::json &rule = RequireObject(inheritance, entry.first, path);
    InheritanceRule &out = settings_.inheritance[static_cast<size_t>(entry.second)];
    out.slots = static_cast<int>(RequireNumber(rule, kKeySlots, path));
    out.rarity = RequireNumber(rule, kKeyRarity, path);
    if (out.slots < 0 || out.rarity <= 0.0f) {
      FailInvalid(path, std::string("inheritance.") + entry.first + " needs slots >= 0 and rarity > 0");
    }
  }
}
