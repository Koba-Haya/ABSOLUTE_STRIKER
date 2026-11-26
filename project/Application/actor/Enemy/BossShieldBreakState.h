#pragma once
#include "BossShieldStageState.h"

// ボスの状態: シールドの成功(砕けた)。溜めの演出を止めて、ボスがひるむ。少しの間、攻撃しない(自機の攻撃のチャンス)。
// 次へ進む条件: ひるむ時間が過ぎた -> シールドを終え、フェーズを進めて、待機(BossIdleState)へ
class BossShieldBreakState : public BossShieldStageState {
public:
    // nextPhase: シールドが終わったときに進めるフェーズ。耐久は 0(壊れた)
    explicit BossShieldBreakState(BossPhase nextPhase) : BossShieldStageState(nextPhase, kBrokenShieldHp) {}

    void Enter(IBossActions& actions) override;

private:
    float Seconds_() const override { return BossShieldDefaults::kBreakStaggerSeconds; }
    BossShieldStage ViewStage_() const override { return BossShieldStage::Break; }
    void OnTimeUp_(IBossActions& actions) override { Finish_(actions); }

    static constexpr int kBrokenShieldHp = 0;
};
