#include "BossAttackState.h"
#include "BossActions.h"
#include "BossAttackPatternFactory.h"
#include "BossIdleState.h"
#include <memory>

BossAttackState::BossAttackState(const BossAttackSpec& spec, std::size_t attackIndex)
    : spec_(spec), pattern_(BossAttackPatternFactory::Create(spec)), attackIndex_(attackIndex) {
    // 演出へ渡す情報: 段の長さは表から、輪の見せ方はパターンから決める
    cue_.telegraphSec = spec_.telegraphSec;
    cue_.chargeSec = spec_.chargeSec;
    cue_.ring = pattern_->GetMuzzleRingStyle();
}

void BossAttackState::Enter(IBossActions& actions) {
    stage_ = Stage::Telegraph;
    timer_ = 0.0f;
    // 予告の始まりを伝える(体は、予告のアニメーション = 構えへ移る動き、に切り替える)
    actions.SetAttackStage(BossAttackStage::Telegraph, cue_);
    // 攻撃ごとの準備(誘導弾なら、玉をボスの周りへ出す)
    pattern_->OnTelegraphStart(actions);
}

void BossAttackState::Exit(IBossActions& actions) {
    // 攻撃が終わる(次の状態へ移る、または、ほかの理由で抜ける)ことを伝える
    actions.SetAttackStage(BossAttackStage::None, cue_);
    // 攻撃ごとの後片付け(誘導弾なら、発射の前に中断されたときに、待っている玉を消す)
    pattern_->OnEnd(actions);
}

float BossAttackState::GetStageSec() const {
    switch (stage_) {
    case Stage::Telegraph: return spec_.telegraphSec;
    case Stage::Charge:    return spec_.chargeSec;
    case Stage::Recover:   return spec_.recoverSec;
    }
    return 0.0f;
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
            actions.SetAttackStage(BossAttackStage::Charge, cue_);
            break;
        case Stage::Charge:
            // 溜めが終わった瞬間に、発射のアニメーションと演出へ切り替えて、弾を撃つ
            actions.SetAttackStage(BossAttackStage::Fire, cue_);
            pattern_->Fire(actions);
            stage_ = Stage::Recover;
            break;
        case Stage::Recover:
            // 隙が終わったら、待機の状態へ戻る(次の攻撃は、表の次の番号)
            actions.ChangeState(std::make_unique<BossIdleState>(attackIndex_ + 1));
            return;
        }
    }
}
