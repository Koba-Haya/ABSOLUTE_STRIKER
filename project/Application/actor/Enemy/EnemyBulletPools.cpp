#include "EnemyBulletPools.h"

#include "AbsoluteEngine/scene/GameObjectPool.h"

EnemyBulletPools::EnemyBulletPools() = default;

EnemyBulletPools::~EnemyBulletPools() = default;

AbsoluteEngine::GameObjectPool& EnemyBulletPools::Get(const std::string& bulletId, bool* created) {
  auto it = pools_.find(bulletId);
  const bool isNew = (it == pools_.end());
  if (isNew) {
    it = pools_.emplace(bulletId, std::make_unique<AbsoluteEngine::GameObjectPool>()).first;
  }
  if (created) {
    *created = isNew;
  }
  return *it->second;
}
