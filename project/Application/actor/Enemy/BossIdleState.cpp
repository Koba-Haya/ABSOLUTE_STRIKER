#include "BossIdleState.h"
#include "BossActions.h"
#include "BossAttackPlan.h"
#include "BossAttackState.h"
#include <memory>

void BossIdleState::Enter(IBossActions& actions) {
    // 待つ時間は、これから行う攻撃の表から決める(フェーズが変わっていれば、新しいフェーズの表を見る)
    waitSec_ = PickBossAttack(actions.GetPhase(), attackIndex_).idleSec;
    timer_ = 0.0f;
}

void BossIdleState::Update(IBossActions& actions, float deltaTime) {
    timer_ += deltaTime;
    if (timer_ < waitSec_) return;

    // 待機が終わったので、攻撃の状態へ進む。攻撃は、この時点のフェーズの表から取る
    const BossAttackSpec& spec = PickBossAttack(actions.GetPhase(), attackIndex_);
    actions.ChangeState(std::make_unique<BossAttackState>(spec, attackIndex_));
}
