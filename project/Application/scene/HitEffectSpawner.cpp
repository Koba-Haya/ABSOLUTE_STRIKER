#include "HitEffectSpawner.h"
#include "BaseScene.h"
#include "Renderer.h"
#include "Method.h"
#include "ParticleManager.h"
#include "particle/GPUParticleManager.h"
#include "graphics/particle/EffectManager.h"
#include "graphics/particle/RingEffect.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/LightNodeComponent.h"
#include "component/ExplosionLightComponent.h"
#include <algorithm>
#include <cstdlib>

void HitEffectSpawner::Initialize() {
  auto* am = AbsoluteEngine::AssetManager::GetInstance();
  resEffect_ = am->Load<ModelResource>("resources/app/particle/particle.obj");
  texRing_   = am->Load<TextureResource>("resources/app/textures/gradationLine.png");
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

  // 1.5. GPU Particle（加点要素）: 敵撃破のたびに並列化されたEmit CSでバーストを発生させる
  GPUParticleManager::GetInstance()->EmitBurst(pos, 20);

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
  lightNodeComp->radius = 30.0f;
  lightNodeComp->decay = 1.0f;
  lightObj->AddComponent(std::move(lightNodeComp));
  auto lightComp = std::make_unique<ExplosionLightComponent>();
  lightComp->maxIntensity_ = 20.0f;
  lightComp->lifeTime_ = 0.5f;
  lightObj->AddComponent(std::move(lightComp));
  scene.AddRootObject(lightObj);

  // 4. 旧型スケールアニメーションのヒットエフェクト（後方互換）
  ScaleEffect ef;
  ef.instance.Initialize({ resEffect_, {1, 1, 1, 1}, 0 });
  ef.position = pos;
  ef.frame = 0.0f;
  ef.isActive = true;
  scaleEffects_.push_back(std::move(ef));
}

void HitEffectSpawner::Update() {
  for (auto& ef : scaleEffects_) {
      if (!ef.isActive) continue;
      ef.frame += 1.0f;
      float t = ef.frame / ef.maxFrame;
      float scaleVal = t * 4.0f;
      ef.instance.SetWorld(MakeAffineMatrix(Vector3{scaleVal, scaleVal, scaleVal}, Vector3{0.0f, 0.0f, 0.0f}, ef.position));
      ef.instance.SetColor({1.0f, 0.5f, 0.0f, 1.0f - t});
      if (ef.frame >= ef.maxFrame) ef.isActive = false;
  }
  scaleEffects_.erase(std::remove_if(scaleEffects_.begin(), scaleEffects_.end(),
      [](const ScaleEffect& e) { return !e.isActive; }), scaleEffects_.end());
}

void HitEffectSpawner::Draw(Renderer* renderer) {
  for (auto& ef : scaleEffects_) {
      renderer->DrawEffectModel(&ef.instance);
  }
}
