#pragma once
#include "BossShieldStageState.h"

// ボスの状態: シールドの維持。制限時間の間、ボスは力を溜める(攻撃の演出は「溜め」を借りる)。
// 自機の攻撃は、ボスの HP ではなく、シールドの耐久を減らす。
// 次へ進む条件: 耐久が 0 になった -> BossShieldBreakState(成功)。制限時間が過ぎた -> BossShieldBurstState(失敗)
class BossShieldHoldState : public BossShieldStageState {
public:
    // nextPhase: シールドが終わったときに進めるフェーズ。shieldHp: 始まりの耐久
    BossShieldHoldState(BossPhase nextPhase, int shieldHp) : BossShieldStageState(nextPhase, shieldHp) {}

    void Enter(IBossActions& actions) override { EnterStage_(actions, BossAttackStage::Charge); }

private:
    float Seconds_() const override { return BossShieldDefaults::kTimeLimitSeconds; }
    BossShieldStage ViewStage_() const override { return BossShieldStage::Hold; }
    // 時間切れ: 失敗の爆発へ
    void OnTimeUp_(IBossActions& actions) override;
    // 耐久を減らし、0 になったら成功へ
    void OnShieldHit_(IBossActions& actions, int damage) override;
    // 残り時間の割合と、時間切れの直前の警告
    void FillTime_(BossShieldView& view) const override;
};
