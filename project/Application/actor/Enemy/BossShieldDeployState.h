#pragma once
#include "BossShieldStageState.h"

// ボスの状態: シールドの展開。HP が次のフェーズの境目を切ったときに、待機や攻撃に割り込んで始まる(IBossState::OnPhaseBoundaryCrossed)。
// 攻撃を中断してシールドを張る。この間は、攻撃を受けても何も起きない。攻撃の演出は「予告」を借りる。
// 次へ進む条件: 展開の時間が過ぎた -> BossShieldHoldState(維持)
class BossShieldDeployState : public BossShieldStageState {
public:
    // nextPhase: シールドが終わったときに進めるフェーズ。耐久は満タンから始める
    explicit BossShieldDeployState(BossPhase nextPhase);

    // ボスへ、シールドを張り始めたことを伝え(HP を境目へ戻す)、予告の演出を始める
    void Enter(IBossActions& actions) override;

private:
    float Seconds_() const override { return BossShieldDefaults::kDeploySeconds; }
    BossShieldStage ViewStage_() const override { return BossShieldStage::Deploy; }
    void OnTimeUp_(IBossActions& actions) override;
    // 展開の間は、残り時間の表示を満タンにしておく(維持が始まると減り始める)
    void FillTime_(BossShieldView& view) const override { view.timeRatio = kFullTimeRatio; }

    static constexpr float kFullTimeRatio = 1.0f;
};
