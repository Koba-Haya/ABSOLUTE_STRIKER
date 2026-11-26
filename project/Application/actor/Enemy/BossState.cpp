#include "BossState.h"
#include "BossActions.h"
#include "BossShieldDeployState.h"
#include <memory>

void IBossState::OnPhaseBoundaryCrossed(IBossActions& actions, BossPhase nextPhase) {
    // 待機や攻撃の途中でも、シールドの展開へ進む(攻撃は、Exit で片づく)
    actions.ChangeState(std::make_unique<BossShieldDeployState>(nextPhase));
}
