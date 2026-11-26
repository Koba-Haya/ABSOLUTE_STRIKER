#include "BossShieldDeployState.h"
#include "BossShieldHoldState.h"
#include <memory>

BossShieldDeployState::BossShieldDeployState(BossPhase nextPhase) : BossShieldStageState(nextPhase, BossShieldDefaults::kShieldHp) {}

void BossShieldDeployState::Enter(IBossActions& actions) {
    actions.BeginShield(GetNextPhase());
    EnterStage_(actions, BossAttackStage::Telegraph);
}

void BossShieldDeployState::OnTimeUp_(IBossActions& actions) {
    actions.ChangeState(std::make_unique<BossShieldHoldState>(GetNextPhase(), GetShieldHp()));
}
