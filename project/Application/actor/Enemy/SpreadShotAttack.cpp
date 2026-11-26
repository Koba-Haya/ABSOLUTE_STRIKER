#include "SpreadShotAttack.h"
#include "BossActions.h"
#include <algorithm>

namespace {
// 扇状弾のとき、輪の大きさに掛ける倍率(以前 BossAttackEffects にあった値と同じ)
constexpr float kSpreadRingScale = 1.25f;
// 弧の数の下限(弾の数が 0 以下に設定されても、輪は描けるようにする)
constexpr int kMinArcCount = 1;
} // namespace

void SpreadShotAttack::Fire(IBossActions& actions) const {
    actions.FireSpreadShot(count_, spreadAngleDeg_, speed_);
}

BossMuzzleRingStyle SpreadShotAttack::GetMuzzleRingStyle() const {
    BossMuzzleRingStyle style;
    style.arcCount = static_cast<uint32_t>((std::max)(count_, kMinArcCount)); // max マクロとの衝突を避ける括弧
    style.scaleMultiplier = kSpreadRingScale;
    return style;
}
