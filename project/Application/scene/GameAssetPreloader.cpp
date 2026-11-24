#include "GameAssetPreloader.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "AbsoluteEngine/resources/AssetManager.h"
#include "CpuTimer.h"
#include "ModelResource.h"
#include "ParticleManager.h"
#include "TextureResource.h"

namespace {
// 先読みする資産のうち、ゲームシーン専用で他のクラス内にパスが書かれているもの。
// (HitEffectSpawner と、Game.json から参照されるプレハブ用の立方体)
constexpr const char *kHitEffectRingTexture = "resources/app/textures/gradationLine.png";
constexpr const char *kPrefabCubeModel = "resources/app/cube/cube.obj";
constexpr const char *kPrefabCubeTexture = "resources/app/cube/white100x100.png";

template <typename T> void Hold(std::vector<std::shared_ptr<T>> &list, std::shared_ptr<T> res) {
  // 読み込みに失敗した(nullptr)ものは保持しない
  if (res) {
    list.push_back(std::move(res));
  }
}
} // namespace

void GameAssetPreloader::Preload() {
  if (loaded_) {
    return;
  }
  CpuTimer::Scope scope("GameAssetPreloader.Preload");

  auto *am = AbsoluteEngine::AssetManager::GetInstance();

  // モデル
  Hold(models_, am->Load<ModelResource>(GameAssetPaths::kPlayerModel));
  Hold(models_, am->Load<ModelResource>(kPrefabCubeModel));

  // テクスチャ
  Hold(textures_, am->Load<TextureResource>(GameAssetPaths::kSkyboxDds));
  Hold(textures_, am->Load<TextureResource>(GameAssetPaths::kDissolveNoise));
  Hold(textures_, am->Load<TextureResource>(kHitEffectRingTexture));
  Hold(textures_, am->Load<TextureResource>(kPrefabCubeTexture));

  // パーティクルグループ。ParticleManager は作ったグループを持ち続け、
  // ゲームシーン側の同名の作成は「リセットして再利用」になるので、ここで先に作っておくと安い
  ParticleManager::GetInstance()->CreateParticleGroup(GameParticleGroups::kDefaultName, GameAssetPaths::kDefaultParticleTexture,
                                                      GameParticleGroups::kDefaultMax);
  ParticleManager::GetInstance()->CreateParticleGroup(GameParticleGroups::kExplosionName, GameAssetPaths::kExplosionParticleTexture,
                                                      GameParticleGroups::kExplosionMax);

  loaded_ = true;
}

void GameAssetPreloader::Release() {
  models_.clear();
  textures_.clear();
  loaded_ = false;
}
