#include "LockOnTarget.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../Enemy/BossComponent.h"
#include "../Enemy/BossOrbComponent.h"
#include "../Enemy/BossOrbTags.h"
#include "../Enemy/EnemyComponent.h"

namespace {
// 通常の敵とボスのタグ(GameObject::GetTag の値)
constexpr const char* kEnemyTag = "Enemy";
constexpr const char* kBossTag = "Boss";
} // namespace

bool LockOnTarget::IsTargetTag(const std::string& tag) {
    return tag == kEnemyTag || tag == kBossTag || tag == BossOrbTags::kOrb;
}

bool LockOnTarget::IsAlive(AbsoluteEngine::GameObject& target) {
    if (const auto* enemy = target.GetComponent<EnemyComponent>()) return !enemy->IsDead();
    if (const auto* boss = target.GetComponent<BossComponent>()) return boss->GetHp() > 0;
    if (const auto* orb = target.GetComponent<BossOrbComponent>()) return orb->IsTargetable();
    return true; // 生死を表す部品を持たない対象(開発用に置いた敵など)は、ロックオンの検索や外す判定で、これまでどおり、生きているものとして扱う
}
