#include "BossShieldBreakState.h"

void BossShieldBreakState::Enter(IBossActions& actions) {
    // 成功: 溜めの演出を止めて、ボスがひるむ
    EnterStage_(actions, BossAttackStage::None);
    actions.Stagger();
}
