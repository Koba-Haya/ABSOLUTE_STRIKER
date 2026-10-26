#include "RamAttackComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "EnemyComponent.h"
#include "../../actor/Player/PlayerComponent.h"

void RamAttackComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!owner_ || !other) return;

    auto* player = other->GetComponent<PlayerComponent>();
    if (!player) return;
    // 死んだ(死亡演出中の)自機には体当たりしない。自爆すると、撃破の演出・スコア・編隊のカウントが、死亡演出の最中に出てしまう
    if (player->IsDead()) return;

    // プレイヤーに衝突した瞬間に確実に自爆撃破する（現在のHPに関わらず）
    if (auto* enemyComp = owner_->GetComponent<EnemyComponent>()) {
        enemyComp->OnHit(enemyComp->GetMaxHp());
    }
}
