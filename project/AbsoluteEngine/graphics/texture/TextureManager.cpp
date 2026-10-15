#include "TextureManager.h"
#include "TextureResource.h"
#include <Windows.h>

#include <filesystem>
#include "../../base/AssetPath.h"
static void TexFatal_(const std::string &msg) {
  MessageBoxA(nullptr, msg.c_str(), "Texture Fatal", MB_OK | MB_ICONERROR);
}

std::shared_ptr<TextureResource> TextureManager::Load(const std::string &path) {
  if (!dx_) {
    TexFatal_("[TextureManager] dx_ is null. Call Initialize first.");
  }

  if (auto it = cache_.find(path); it != cache_.end()) {
    if (auto alive = it->second.lock()) {
      return alive;
    }
  }

  auto tex = std::make_shared<TextureResource>();

  // exe 基準の実パスに解決する。見つからない場合は、要求されたパスと
  // 実際に探した場所の両方を表示して止める(配置ミスの原因を特定しやすくするため)
  const std::filesystem::path resolvedPath = AbsoluteEngine::AssetPath::Resolve(path);
  if (!std::filesystem::exists(resolvedPath)) {
    TexFatal_(std::string("[TextureManager] file not found:\n") + path +
              "\n\nsearched:\n" + resolvedPath.string());
    return nullptr;
  }

  // 実際の読み込み側(LoadTexture)でも同じ規則で解決するため、元のパスのまま渡す
  const bool ok = tex->CreateFromFile(dx_, path);
  if (!ok) {
    TexFatal_(std::string("[TextureManager] CreateFromFile failed:\n") + resolvedPath.string());
    return nullptr;
  }

  cache_[path] = tex;
  return tex;
}

void TextureManager::ClearUnused() {
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (it->second.expired()) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}
