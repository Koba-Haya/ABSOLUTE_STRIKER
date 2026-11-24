#include "BossAttackState.h"
#include "BossActions.h"
#include "BossIdleState.h"
#include <memory>

void BossAttackState::Enter(IBossActions& actions) {
    stage_ = Stage::Telegraph;
    timer_ = 0.0f;
    // 予告の始まりを伝える(体は、予告のアニメーション = 構えへ移る動き、に切り替える)
    actions.SetAttackStage(BossAttackStage::Telegraph, spec_);
}

void BossAttackState::Exit(IBossActions& actions) {
    // 攻撃が終わる(次の状態へ移る、または、ほかの理由で抜ける)ことを伝える
    actions.SetAttackStage(BossAttackStage::None, spec_);
}

float BossAttackState::GetStageSec() const {
    switch (stage_) {
    case Stage::Telegraph: return spec_.telegraphSec;
    case Stage::Charge:    return spec_.chargeSec;
    case Stage::Recover:   return spec_.recoverSec;
    }
    return 0.0f;
}

void BossAttackState::Fire(IBossActions& actions) const {
    switch (spec_.kind) {
    case BossAttackKind::Aimed:
        actions.FireAimedShot(spec_.speed);
        break;
    case BossAttackKind::Spread:
        actions.FireSpreadShot(spec_.count, spec_.spreadAngleDeg, spec_.speed);
        break;
    }
}

void BossAttackState::Update(IBossActions& actions, float deltaTime) {
    timer_ += deltaTime;

    // 段の秒数を使い切ったら、次の段へ進む。秒数 0 の段があっても、1 フレームで通り抜けられるよう、繰り返す。
    // 余った時間は、次の段へ持ち越す(フレームの刻みで、段の長さがずれないようにするため)
    while (timer_ >= GetStageSec()) {
        timer_ -= GetStageSec();
        switch (stage_) {
        case Stage::Telegraph:
            stage_ = Stage::Charge;
            actions.SetAttackStage(BossAttackStage::Charge, spec_);
            break;
        case Stage::Charge:
            // 溜めが終わった瞬間に、発射のアニメーションと演出へ切り替えて、弾を撃つ
            actions.SetAttackStage(BossAttackStage::Fire, spec_);
            Fire(actions);
            stage_ = Stage::Recover;
            break;
        case Stage::Recover:
            // 隙が終わったら、待機の状態へ戻る(次の攻撃は、表の次の番号)
            actions.ChangeState(std::make_unique<BossIdleState>(attackIndex_ + 1));
            return;
        }
    }
}
