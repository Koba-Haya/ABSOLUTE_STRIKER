#include "BulletFactory.h"
#include "BulletTag.h"
#include "HomingBulletComponent.h"
#include "RibbonTrailComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"

namespace {
// ホーミング弾の尾の色。背景が青い空なので、水色系ではなく暖色にして目立たせる(加算合成で、芯は白く光る)
constexpr Vector4 kHomingTrailColor = { 1.0f, 0.55f, 0.15f, 1.0f };

// ホーミング弾の GameObject の名前
constexpr const char* kHomingBulletName = "PlayerHomingBullet";

// ホーミング弾の尾の設定(間隔・寿命・幅などは RibbonTrail::Params の既定値を使い、色だけ変える)
RibbonTrail::Params MakeHomingTrailParams() {
  RibbonTrail::Params params;
  params.color = kHomingTrailColor;
  return params;
}
} // namespace

namespace BulletFactory {

std::shared_ptr<AbsoluteEngine::GameObject> CreatePlayerHoming(const HomingBulletSpec& spec, AbsoluteEngine::ISpawner& spawner) {
  // 弾自体は描かない(モデルを付けない)。尾のリボンだけで見せる
  auto bulletObj = std::make_shared<AbsoluteEngine::GameObject>(kHomingBulletName);
  bulletObj->SetTag(BulletTag::kPlayer);

  AbsoluteEngine::ColliderComponent::Settings colliderSettings;
  colliderSettings.type   = AbsoluteEngine::ColliderComponent::Type::Sphere;
  colliderSettings.radius = spec.radius;
  bulletObj->AddComponent(std::make_unique<AbsoluteEngine::ColliderComponent>(colliderSettings));
  bulletObj->SetPosition(spec.startPos);

  auto homingComp = std::make_unique<HomingBulletComponent>();
  homingComp->Initialize(spec.startPos, spec.controlPos, spec.duration, spec.target);
  bulletObj->AddComponent(std::move(homingComp));

  // 軌跡の記録。弾の移動(HomingBulletComponent)のあとに Update されるよう、後から追加する
  auto trailComp = std::make_unique<RibbonTrailComponent>(spawner);
  trailComp->Initialize(MakeHomingTrailParams());
  bulletObj->AddComponent(std::move(trailComp));
  return bulletObj;
}

} // namespace BulletFactory
