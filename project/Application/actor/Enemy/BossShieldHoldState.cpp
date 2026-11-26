#include "BossShieldHoldState.h"
#include "BossShieldBreakState.h"
#include "BossShieldBurstState.h"
#include <memory>

namespace {
// 耐久がなくなった値
constexpr int kShieldBroken = 0;
// 残り時間の割合の満タン
constexpr float kFullTimeRatio = 1.0f;
} // namespace

void BossShieldHoldState::OnTimeUp_(IBossActions& actions) {
    actions.ChangeState(std::make_unique<BossShieldBurstState>(GetNextPhase(), GetShieldHp()));
}

void BossShieldHoldState::OnShieldHit_(IBossActions& actions, int damage) {
    // すでに壊れていれば(同じフレームの、成功への切り替えを待つ間に、もう 1 発当たった)、何もしない
    if (GetShieldHp() <= kShieldBroken) return;

    const int remaining = (std::max)(GetShieldHp() - damage, kShieldBroken);
    SetShieldHp(remaining);
    if (remaining <= kShieldBroken) {
        actions.ChangeState(std::make_unique<BossShieldBreakState>(GetNextPhase())); // 成功
    } else {
        PublishView_(actions);
    }
}

void BossShieldHoldState::FillTime_(BossShieldView& view) const {
    view.timeRatio = kFullTimeRatio - GetStageProgress();
    view.warning = (BossShieldDefaults::kTimeLimitSeconds - GetTimer()) <= BossShieldDefaults::kWarningSeconds;
}
