#pragma once
#include "BossShieldStageState.h"

// ボスの状態: シールドの失敗(時間切れの爆発)。全画面が光る爆発が起き、自機がダメージを受ける(回避の無敵の間は受けない)。
// 攻撃の演出は「発射」(閃光、火花)を借りる。爆発は、この段に入った瞬間に起きる。
// 次へ進む条件: 爆発の段の時間が過ぎた -> シールドを終え、フェーズを進めて、待機(BossIdleState)へ
class BossShieldBurstState : public BossShieldStageState {
public:
    // nextPhase: シールドが終わったときに進めるフェーズ。shieldHp: 残っていた耐久(外へ見せる表示に使う)
    BossShieldBurstState(BossPhase nextPhase, int shieldHp) : BossShieldStageState(nextPhase, shieldHp) {}

    void Enter(IBossActions& actions) override;

private:
    float Seconds_() const override { return BossShieldDefaults::kBurstSeconds; }
    BossShieldStage ViewStage_() const override { return BossShieldStage::Burst; }
    void OnTimeUp_(IBossActions& actions) override { Finish_(actions); }
};
