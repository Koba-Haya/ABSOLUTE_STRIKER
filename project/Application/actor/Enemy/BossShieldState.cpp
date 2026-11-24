#include "BossShieldState.h"
#include "BossActions.h"
#include "BossIdleState.h"
#include "Logger.h"
#include <algorithm>
#include <memory>

namespace {
using namespace BossShieldDefaults;
}

BossShieldState::BossShieldState(BossPhase nextPhase) : nextPhase_(nextPhase) {
    // 攻撃の演出(輪、粒)に渡す定義: 展開を予告、維持を溜めとして見せるので、その長さを入れる。弾は撃たない
    effectSpec_.kind = BossAttackKind::Aimed;
    effectSpec_.telegraphSec = kDeploySeconds;
    effectSpec_.chargeSec = kTimeLimitSeconds;
}

void BossShieldState::Enter(IBossActions& actions) {
    shieldHp_ = kShieldHp;
    EnterStage_(actions, Stage::Deploy);
}

float BossShieldState::GetStageSeconds_() const {
    switch (stage_) {
    case Stage::Deploy: return kDeploySeconds;
    case Stage::Hold:   return kTimeLimitSeconds;
    case Stage::Break:  return kBreakStaggerSeconds;
    case Stage::Burst:  return kBurstSeconds;
    }
    return 0.0f;
}

void BossShieldState::EnterStage_(IBossActions& actions, Stage stage) {
    stage_ = stage;
    timer_ = 0.0f;
    // 段の切り替えを、ログに残す(シールドの流れを、実機で確認するため)
    Logger::GetInstance().Log("[BossShield] stage=%d shieldHp=%d nextPhase=%d", static_cast<int>(stage), shieldHp_, static_cast<int>(nextPhase_));
    switch (stage) {
    case Stage::Deploy:
        actions.SetAttackStage(BossAttackStage::Telegraph, effectSpec_);
        break;
    case Stage::Hold:
        actions.SetAttackStage(BossAttackStage::Charge, effectSpec_);
        break;
    case Stage::Break:
        // 成功: 溜めの演出を止めて、ボスがひるむ
        actions.SetAttackStage(BossAttackStage::None, effectSpec_);
        actions.Stagger();
        break;
    case Stage::Burst:
        // 失敗: 発射の演出(閃光、火花)を出して、全画面の爆発のダメージを与える(回避の無敵の間は受けない)
        actions.SetAttackStage(BossAttackStage::Fire, effectSpec_);
        actions.ApplyBlastDamage(kBlastDamage);
        break;
    }
    PublishView_(actions);
}

void BossShieldState::Update(IBossActions& actions, float deltaTime) {
    timer_ += deltaTime;
    if (timer_ >= GetStageSeconds_()) {
        switch (stage_) {
        case Stage::Deploy:
            EnterStage_(actions, Stage::Hold);
            return;
        case Stage::Hold:
            // 時間切れ
            EnterStage_(actions, Stage::Burst);
            return;
        case Stage::Break:
        case Stage::Burst:
            Finish_(actions);
            return;
        }
    }
    PublishView_(actions);
}

bool BossShieldState::AbsorbDamage(IBossActions& actions, int damage) {
    // 維持の間だけ、耐久を減らす。ほかの段は、ダメージを受け止めるだけ(何も減らない)
    if (stage_ == Stage::Hold) {
        shieldHp_ -= damage;
        if (shieldHp_ <= 0) {
            shieldHp_ = 0;
            EnterStage_(actions, Stage::Break); // 成功
        } else {
            PublishView_(actions);
        }
    }
    return true;
}

void BossShieldState::PublishView_(IBossActions& actions) const {
    BossShieldView view;
    switch (stage_) {
    case Stage::Deploy: view.stage = BossShieldStage::Deploy; break;
    case Stage::Hold:   view.stage = BossShieldStage::Hold; break;
    case Stage::Break:  view.stage = BossShieldStage::Break; break;
    case Stage::Burst:  view.stage = BossShieldStage::Burst; break;
    }
    view.hpRatio = static_cast<float>(shieldHp_) / static_cast<float>(kShieldHp);
    const float seconds = GetStageSeconds_();
    view.stageProgress = (seconds > 0.0f) ? std::clamp(timer_ / seconds, 0.0f, 1.0f) : 1.0f;
    if (stage_ == Stage::Hold) {
        view.timeRatio = 1.0f - view.stageProgress;
        view.warning = (kTimeLimitSeconds - timer_) <= kWarningSeconds;
    } else {
        view.timeRatio = (stage_ == Stage::Deploy) ? 1.0f : 0.0f;
    }
    actions.SetShieldView(view);
}

void BossShieldState::Finish_(IBossActions& actions) {
    // 攻撃の演出と、外へ見せる状態を片づけて、フェーズを進め、待機の状態へ戻る
    actions.SetAttackStage(BossAttackStage::None, effectSpec_);
    actions.SetShieldView(BossShieldView{});
    actions.FinishShield(nextPhase_);
    actions.ChangeState(std::make_unique<BossIdleState>(0));
}
