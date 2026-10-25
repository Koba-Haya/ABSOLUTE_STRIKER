#include "HitEffectSpawner.h"
#include "BaseScene.h"
#include "ParticleManager.h"
#include "graphics/particle/EffectManager.h"
#include "graphics/particle/RingEffect.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/LightNodeComponent.h"
#include "component/ExplosionLightComponent.h"
#include <cstdlib>

namespace {
// 爆発の光の寿命(秒)と、生成直後の強さ・届く半径。
// 体当たりの敵が自機のそばで自爆したとき、蝶がオレンジに塗られて見づらかったので、
// 強さを 20 から 10、半径を 30 から 15 に、それぞれ半分にした(さらに調整するなら、この 2 つ)
constexpr float kExplosionLightLifeTime = 0.5f;
constexpr float kExplosionLightMaxIntensity = 10.0f;
constexpr float kExplosionLightRadius = 15.0f;
} // namespace

void HitEffectSpawner::Initialize() {
  auto* am = AbsoluteEngine::AssetManager::GetInstance();
  texRing_ = am->Load<TextureResource>("resources/app/textures/gradationLine.png");
}

// DevScene の実装を移植。
// パーティクル + RingEffect + ExplosionLight の3段構えのエフェクト。
void HitEffectSpawner::Spawn(BaseScene& scene, const Vector3& pos) {
  // 1. 爆発パーティクル（explosion グループ）を散布
  for (int i = 0; i < 30; ++i) {
      float rX = ((rand() % 200) / 100.0f) - 1.0f;
      float rY = ((rand() % 200) / 100.0f) - 1.0f;
      float rZ = ((rand() % 200) / 100.0f) - 1.0f;
      Vector3 vel = { rX * 1.5f, rY * 1.5f, rZ * 1.5f };
      ParticleManager::GetInstance()->Emit(
          "explosion", pos, vel,
          Vector3{ 1.5f, 1.5f, 1.5f }, Vector3{ 0, 0, 0 }, 0.7f, Vector4{ 1, 1, 1, 1 });
  }

  // 2. リングエフェクト
  if (texRing_) {
      EffectManager::GetInstance()->AddEffect(std::make_unique<RingEffect>(texRing_, pos));
  }

  // 3. 爆発のポイントライト（寿命付き）を生成
  auto lightObj = std::make_shared<AbsoluteEngine::GameObject>("ExplosionLight");
  lightObj->GetTransform().translate = pos;
  auto lightNodeComp = std::make_unique<AbsoluteEngine::LightNodeComponent>();
  lightNodeComp->type = AbsoluteEngine::LightNodeComponent::Type::Point;
  lightNodeComp->color = { 1.0f, 0.5f, 0.1f };  // オレンジ色の爆発光
  lightNodeComp->radius = kExplosionLightRadius;
  lightNodeComp->decay = 1.0f;
  lightObj->AddComponent(std::move(lightNodeComp));
  lightObj->AddComponent(std::make_unique<ExplosionLightComponent>(kExplosionLightLifeTime, kExplosionLightMaxIntensity));
  scene.AddRootObject(lightObj);
}
