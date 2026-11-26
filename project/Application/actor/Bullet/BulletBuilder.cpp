#include "BulletBuilder.h"

#include "BulletComponent.h"
#include "BulletDefinition.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/GameObjectPool.h"
#include "AbsoluteEngine/scene/ModelComponent.h"

namespace BulletBuilder {

std::unique_ptr<AbsoluteEngine::GameObject> Build(const BulletDefinition &def, const char *tag) {
  // 名前は定義の id(エディタのヒエラルキーで、どの種類の弾かが分かるように)
  auto bullet = std::make_unique<AbsoluteEngine::GameObject>(def.id);
  bullet->SetTag(tag);

  // 見た目。モデルが無い定義は、見た目を別の部品が描く弾なので、何も付けない
  if (!def.modelPath.empty()) {
    auto model = std::make_unique<AbsoluteEngine::ModelComponent>();
    model->LoadModel(def.modelPath);
    if (def.color) model->SetColor(*def.color);
    bullet->AddComponent(std::move(model));
  }
  bullet->SetScale({def.scale, def.scale, def.scale});

  // 当たり判定。衝突判定は拡大率を掛けないので、定義の半径がそのまま当たりの大きさになる
  AbsoluteEngine::ColliderComponent::Settings collider;
  collider.type = AbsoluteEngine::ColliderComponent::Type::Sphere;
  collider.radius = def.radius;
  bullet->AddComponent(std::make_unique<AbsoluteEngine::ColliderComponent>(collider));

  // 動き(まっすぐ飛ぶ)。撃ち落とせるかは定義で決まるので、組み立てのときに 1 回だけ入れる
  auto motion = std::make_unique<BulletComponent>();
  motion->SetCanBeShotDown(def.canBeShotDown);
  bullet->AddComponent(std::move(motion));

  return bullet;
}

void Launch(AbsoluteEngine::GameObject &bullet, const Vector3 &position, const Vector3 &velocity) {
  bullet.SetPosition(position);
  if (auto *motion = bullet.GetComponent<BulletComponent>()) {
    // 速度を入れ、寿命の経過を 0、有効の印を立て直す(使い回す弾に、前回の値を残さない)
    motion->Initialize(velocity);
  }
}

std::shared_ptr<AbsoluteEngine::GameObject> AcquireOrBuild(AbsoluteEngine::GameObjectPool &pool, const BulletDefinition &def, const char *tag) {
  if (auto reused = pool.TryAcquire()) {
    return reused;
  }
  return pool.Adopt(Build(def, tag));
}

void Prewarm(AbsoluteEngine::GameObjectPool &pool, const BulletDefinition &def, const char *tag) {
  for (int i = 0; i < def.prewarmCount; ++i) {
    pool.AddIdle(Build(def, tag));
  }
}

} // namespace BulletBuilder
