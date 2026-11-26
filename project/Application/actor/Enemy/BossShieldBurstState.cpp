#include "BossShieldBurstState.h"

void BossShieldBurstState::Enter(IBossActions& actions) {
    // 失敗: 発射の演出(閃光、火花)を出して、全画面の爆発のダメージを与える(回避の無敵の間は受けない)
    EnterStage_(actions, BossAttackStage::Fire);
    actions.ApplyBlastDamage(BossShieldDefaults::kBlastDamage);
}
