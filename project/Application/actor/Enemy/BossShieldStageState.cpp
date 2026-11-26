#include "BossShieldStageState.h"
#include "BossActions.h"
#include "BossIdleState.h"
#include "Logger.h"
#include <algorithm>
#include <memory>

namespace {
// 外へ見せる状態の名札を、ログ用の文字にする(シールドの流れを、実機のログで確認するため)
const char* StageName(BossShieldStage stage) {
    switch (stage) {
    case BossShieldStage::Deploy: return "Deploy";
    case BossShieldStage::Hold:   return "Hold";
    case BossShieldStage::Break:  return "Break";
    case BossShieldStage::Burst:  return "Burst";
    case BossShieldStage::None:
    default:                      return "None";
    }
}
} // namespace

BossShieldStageState::BossShieldStageState(BossPhase nextPhase, int shieldHp) : nextPhase_(nextPhase), shieldHp_(shieldHp) {
    // 攻撃の演出(輪、粒)に渡す情報: 展開を予告、維持を溜めとして見せるので、その長さを入れる。輪は標準の見せ方。弾は撃たない
    effectCue_.telegraphSec = BossShieldDefaults::kDeploySeconds;
    effectCue_.chargeSec = BossShieldDefaults::kTimeLimitSeconds;
}

void BossShieldStageState::Update(IBossActions& actions, float deltaTime) {
    timer_ += deltaTime;
    if (timer_ >= Seconds_()) {
        OnTimeUp_(actions);
        return;
    }
    PublishView_(actions);
}

bool BossShieldStageState::AbsorbDamage(IBossActions& actions, int damage) {
    OnShieldHit_(actions, damage);
    return true;
}

void BossShieldStageState::OnPhaseBoundaryCrossed(IBossActions& actions, BossPhase nextPhase) {
    (void)actions;
    (void)nextPhase;
}

void BossShieldStageState::EnterStage_(IBossActions& actions, BossAttackStage attackStage) {
    Logger::GetInstance().Log("[BossShield] stage=%s shieldHp=%d nextPhase=%d", StageName(ViewStage_()), shieldHp_,
                              static_cast<int>(nextPhase_));
    actions.SetAttackStage(attackStage, effectCue_);
    PublishView_(actions);
}

float BossShieldStageState::GetStageProgress() const {
    const float seconds = Seconds_();
    return (seconds > 0.0f) ? std::clamp(timer_ / seconds, 0.0f, 1.0f) : 1.0f;
}

void BossShieldStageState::PublishView_(IBossActions& actions) const {
    BossShieldView view;
    view.stage = ViewStage_();
    view.hpRatio = static_cast<float>(shieldHp_) / static_cast<float>(BossShieldDefaults::kShieldHp);
    view.stageProgress = GetStageProgress();
    FillTime_(view);
    actions.SetShieldView(view);
}

void BossShieldStageState::Finish_(IBossActions& actions) {
    actions.SetAttackStage(BossAttackStage::None, effectCue_);
    actions.SetShieldView(BossShieldView{});
    actions.FinishShield(nextPhase_);
    actions.ChangeState(std::make_unique<BossIdleState>(0));
}
