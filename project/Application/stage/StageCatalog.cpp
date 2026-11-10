#include "StageCatalog.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <utility>

#include "AbsoluteEngine/base/AssetPath.h"
#include "Logger.h"
#include "externals/nlohmann/json.hpp"

namespace {
// ステージ定義のファイル(exe 基準。resources フォルダごと出力先へ複製される)
constexpr const char *kCatalogPath = "resources/app/stage/stages.json";

// JSON の項目名
constexpr const char *kKeyStages = "stages";
constexpr const char *kKeyId = "id";
constexpr const char *kKeyDisplayName = "displayName";
constexpr const char *kKeyDescription = "description";
constexpr const char *kKeyBackdrop = "backdrop";
constexpr const char *kKeyDataName = "dataName";
constexpr const char *kKeyStageNameTexture = "stageNameTexture";
constexpr const char *kKeyUnlocked = "unlocked";

// 定義が正しくないときの終了。何が悪いかを、探したファイルの場所と一緒に表示する
[[noreturn]] void FailInvalid(const std::string &resolvedPath, const std::string &reason) {
  AbsoluteEngine::AssetPath::ReportMissingAndExit("stage definition is invalid (" + reason + ")", resolvedPath);
}

// 必須の文字列項目を読む。無い、文字列でない、空のときは、理由つきで終了する
std::string ReadRequiredString(const nlohmann::json &entry, const char *key, size_t index, const std::string &resolvedPath) {
  const auto it = entry.find(key);
  if (it == entry.end() || !it->is_string() || it->get<std::string>().empty()) {
    FailInvalid(resolvedPath, "stages[" + std::to_string(index) + "]." + key + " must be a non-empty string");
  }
  return it->get<std::string>();
}

// 背景の種類の名前(stages.json の "backdrop" の文字列)と、コード側の StageBackdrop の対応表。背景を増やすときは、ここに足す
constexpr std::pair<const char *, StageBackdrop> kBackdropNames[] = {
    {"cloudSea", StageBackdrop::CloudSea},
};

// 背景の名前を StageBackdrop にする。表に無い名前のときは false(呼び出し側が、探した値を出して終了する)
bool ParseBackdrop(const std::string &name, StageBackdrop &out) {
  for (const auto &entry : kBackdropNames) {
    if (name == entry.first) {
      out = entry.second;
      return true;
    }
  }
  return false;
}

// ファイル名の元になる名前に、フォルダの区切りが入っていないか(別の場所のファイルを指さないように)
bool HasPathSeparator(const std::string &name) {
  return name.find_first_of("/\\") != std::string::npos;
}
} // namespace

StageCatalog::StageCatalog() {
  Load_();
}

StageCatalog &StageCatalog::GetInstance() {
  static StageCatalog instance;
  return instance;
}

const StageDefinition *StageCatalog::Find(const std::string &stageId) const {
  const auto it = std::find_if(stages_.begin(), stages_.end(),
                               [&stageId](const StageDefinition &stage) { return stage.id == stageId; });
  return it == stages_.end() ? nullptr : &*it;
}

void StageCatalog::Load_() {
  const std::filesystem::path path = AbsoluteEngine::AssetPath::Resolve(kCatalogPath);
  const std::string pathText = path.string();

  std::ifstream file(path);
  if (!file) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("stage definition file", pathText);
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const nlohmann::json::exception &e) {
    FailInvalid(pathText, e.what());
  }

  const auto stagesIt = root.find(kKeyStages);
  if (stagesIt == root.end() || !stagesIt->is_array() || stagesIt->empty()) {
    FailInvalid(pathText, std::string("\"") + kKeyStages + "\" must be a non-empty array");
  }

  std::set<std::string> usedIds; // ID の重複の検出用
  for (size_t i = 0; i < stagesIt->size(); ++i) {
    const nlohmann::json &entry = (*stagesIt)[i];
    if (!entry.is_object()) {
      FailInvalid(pathText, "stages[" + std::to_string(i) + "] must be an object");
    }

    StageDefinition stage;
    stage.id = ReadRequiredString(entry, kKeyId, i, pathText);
    stage.displayName = ReadRequiredString(entry, kKeyDisplayName, i, pathText);
    stage.description = ReadRequiredString(entry, kKeyDescription, i, pathText);
    const std::string backdropName = ReadRequiredString(entry, kKeyBackdrop, i, pathText);
    if (!ParseBackdrop(backdropName, stage.backdrop)) {
      // 未知の名前: 探した値を、ログと終了の表示の両方に出す
      Logger::GetInstance().Log("[Stage] unknown backdrop \"%s\" in stages[%zu] of %s", backdropName.c_str(), i, pathText.c_str());
      FailInvalid(pathText, "stages[" + std::to_string(i) + "].backdrop is unknown: \"" + backdropName + "\"");
    }
    stage.dataName = ReadRequiredString(entry, kKeyDataName, i, pathText);
    // stageNameTexture は省略できる(省略、または空文字は「ステージ名の画像なし」)。書くときは文字列であること
    const auto textureIt = entry.find(kKeyStageNameTexture);
    if (textureIt != entry.end()) {
      if (!textureIt->is_string()) {
        FailInvalid(pathText, "stages[" + std::to_string(i) + "]." + kKeyStageNameTexture + " must be a string");
      }
      stage.stageNameTexture = textureIt->get<std::string>();
    }
    // unlocked は省略できる(省略したステージはロック扱い)
    stage.unlocked = entry.value(kKeyUnlocked, false);

    if (HasPathSeparator(stage.dataName)) {
      FailInvalid(pathText, "stages[" + std::to_string(i) + "].dataName must not contain a path separator");
    }
    if (!usedIds.insert(stage.id).second) {
      FailInvalid(pathText, "duplicate stage id: " + stage.id);
    }
    stages_.push_back(std::move(stage));
  }

  Logger::GetInstance().Log("[Stage] catalog loaded: %zu stage(s) from %s", stages_.size(), pathText.c_str());
}
