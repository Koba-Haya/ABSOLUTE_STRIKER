#include "BulletFactory.h"
#include "BulletComponent.h"
#include "HomingBulletComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"

namespace {
constexpr const char* kBulletModelPath = "resources/app/bullet/bullet.obj";

// 弾モデル + 球コライダーを持つ GameObject の共通部分を組み立てる
std::shared_ptr<AbsoluteEngine::GameObject> CreateBulletBase(
    const std::string& name, const char* tag, float radius,
    const Vector3& position, const std::optional<Vector4>& color) {
  auto bulletObj = std::make_shared<AbsoluteEngine::GameObject>(name);
  bulletObj->SetTag(tag);

  auto modelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
  modelComp->LoadModel(kBulletModelPath);
  if (color) modelComp->SetColor(*color);
  bulletObj->AddComponent(std::move(modelComp));

  auto colliderComp = std::make_unique<AbsoluteEngine::ColliderComponent>();
  colliderComp->type   = AbsoluteEngine::ColliderComponent::Type::Sphere;
  colliderComp->radius = radius;
  bulletObj->AddComponent(std::move(colliderComp));

  bulletObj->GetTransform().translate = position;
  return bulletObj;
}

std::shared_ptr<AbsoluteEngine::GameObject> CreateStraight(
    const BulletSpec& spec, const char* defaultName, const char* tag) {
  auto bulletObj = CreateBulletBase(spec.name.empty() ? defaultName : spec.name,
                                    tag, spec.radius, spec.position, spec.color);
  auto bulletComp = std::make_unique<BulletComponent>();
  bulletComp->Initialize(spec.velocity);
  bulletObj->AddComponent(std::move(bulletComp));
  return bulletObj;
}
} // namespace

namespace BulletFactory {

std::shared_ptr<AbsoluteEngine::GameObject> CreatePlayerNormal(const BulletSpec& spec) {
  return CreateStraight(spec, "PlayerBullet", "PlayerBullet");
}

std::shared_ptr<AbsoluteEngine::GameObject> CreatePlayerHoming(const HomingBulletSpec& spec) {
  auto bulletObj = CreateBulletBase("PlayerHomingBullet", "PlayerBullet",
                                    spec.radius, spec.startPos, std::nullopt);
  auto homingComp = std::make_unique<HomingBulletComponent>();
  homingComp->Initialize(spec.startPos, spec.controlPos, spec.duration, spec.target);
  bulletObj->AddComponent(std::move(homingComp));
  return bulletObj;
}

std::shared_ptr<AbsoluteEngine::GameObject> CreateEnemy(const BulletSpec& spec) {
  return CreateStraight(spec, "EnemyBullet", "EnemyBullet");
}

} // namespace BulletFactory
