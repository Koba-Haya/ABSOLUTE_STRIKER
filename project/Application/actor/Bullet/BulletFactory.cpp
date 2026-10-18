#include "BulletFactory.h"
#include "BulletComponent.h"
#include "HomingBulletComponent.h"
#include "RibbonTrailComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"

namespace {
constexpr const char* kBulletModelPath = "resources/app/bullet/bullet.obj";

// ホーミング弾の尾の色。背景が青い空なので、水色系ではなく暖色にして目立たせる(加算合成で、芯は白く光る)
constexpr Vector4 kHomingTrailColor = { 1.0f, 0.55f, 0.15f, 1.0f };

// ホーミング弾の尾の設定(間隔・寿命・幅などは RibbonTrail::Params の既定値を使い、色だけ変える)
RibbonTrail::Params MakeHomingTrailParams() {
  RibbonTrail::Params params;
  params.color = kHomingTrailColor;
  return params;
}

// 弾の GameObject の共通部分(球コライダー、必要ならモデル)を組み立てる。
// withModel が false のとき、弾自体は描かれない(ホーミング弾は、尾のリボンだけで見せる)
std::shared_ptr<AbsoluteEngine::GameObject> CreateBulletBase(
    const std::string& name, const char* tag, float radius,
    const Vector3& position, const std::optional<Vector4>& color,
    bool withModel = true) {
  auto bulletObj = std::make_shared<AbsoluteEngine::GameObject>(name);
  bulletObj->SetTag(tag);

  if (withModel) {
    auto modelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
    modelComp->LoadModel(kBulletModelPath);
    if (color) modelComp->SetColor(*color);
    bulletObj->AddComponent(std::move(modelComp));
  }

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
                                    spec.radius, spec.startPos, std::nullopt,
                                    /*withModel=*/false);
  auto homingComp = std::make_unique<HomingBulletComponent>();
  homingComp->Initialize(spec.startPos, spec.controlPos, spec.duration, spec.target);
  bulletObj->AddComponent(std::move(homingComp));

  // 軌跡の記録。弾の移動(HomingBulletComponent)のあとに Update されるよう、後から追加する
  auto trailComp = std::make_unique<RibbonTrailComponent>();
  trailComp->Initialize(MakeHomingTrailParams());
  bulletObj->AddComponent(std::move(trailComp));
  return bulletObj;
}

std::shared_ptr<AbsoluteEngine::GameObject> CreateEnemy(const BulletSpec& spec) {
  return CreateStraight(spec, "EnemyBullet", "EnemyBullet");
}

} // namespace BulletFactory
