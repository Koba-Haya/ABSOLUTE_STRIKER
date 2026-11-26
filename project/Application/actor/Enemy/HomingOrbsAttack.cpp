#include "HomingOrbsAttack.h"
#include "BossActions.h"

void HomingOrbsAttack::OnTelegraphStart(IBossActions& actions) const {
    // 予告の始まりに、玉をボスの周りへ出す(予告の間に現れ、溜めの間は、固定で待つ)
    actions.SummonOrbs(count_, telegraphSec_, chargeSec_);
}

void HomingOrbsAttack::Fire(IBossActions& actions) const {
    // 溜めが終わった瞬間に、玉を 1 つずつ自機へ向けて発射する
    actions.LaunchOrbs(flightSec_, launchIntervalSec_);
}

void HomingOrbsAttack::OnEnd(IBossActions& actions) const {
    // 発射の前に攻撃が中断されたとき(シールドの発動など)に、待っている玉が残らないようにする。発射を命じた玉は、そのまま飛び続ける
    actions.DismissOrbs();
}

BossMuzzleRingStyle HomingOrbsAttack::GetMuzzleRingStyle() const {
    BossMuzzleRingStyle style;
    style.visible = false;
    return style;
}
