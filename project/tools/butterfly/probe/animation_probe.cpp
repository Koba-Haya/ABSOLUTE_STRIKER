// AnimationManager の名前指定(LoadAnimationSet / LoadAnimation)を、蝶の glTF で確認する確認用ツール(ゲーム本体には含めない)。
// exe と同じフォルダの resources/app/butterfly/butterfly.gltf を読む(エンジンのパス解決が exe 基準のため)。
#include "AnimationManager.h"
#include <cstdio>

int main() {
  auto* manager = AnimationManager::GetInstance();
  auto set = manager->LoadAnimationSet("resources/app/butterfly", "butterfly.gltf");
  if (!set) { std::printf("LoadAnimationSet FAILED\n"); return 1; }
  std::printf("set size=%zu\n", set->animations.size());
  for (const auto& a : set->animations)
    std::printf("  '%s' duration=%.3f joints=%zu\n", a->name.c_str(), a->duration, a->nodeAnimations.size());

  // 名前指定
  for (const char* name : {"Flap", "Glide", "Hit", "Tumble", "NoSuchAnimation"}) {
    auto a = manager->LoadAnimation("resources/app/butterfly", "butterfly.gltf", name);
    std::printf("by name %-16s -> %s\n", name, a ? "found" : "nullptr");
  }
  // 従来の呼び出し(名前なし)は先頭
  auto first = manager->LoadAnimation("resources/app/butterfly", "butterfly.gltf");
  std::printf("legacy (no name) -> '%s'\n", first ? first->name.c_str() : "nullptr");
  // キャッシュ: 同じ集合が返る
  auto again = manager->LoadAnimationSet("resources/app/butterfly", "butterfly.gltf");
  std::printf("cache same instance: %s\n", again == set ? "yes" : "no");
  // 既存モデル(人)の従来呼び出し
  auto walk = manager->LoadAnimation("resources/app/human", "walk.gltf");
  std::printf("legacy human walk -> %s\n", walk ? "ok" : "nullptr");
  return 0;
}
