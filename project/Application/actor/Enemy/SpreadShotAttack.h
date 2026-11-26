#pragma once
#include "IBossAttackPattern.h"

// 攻撃パターン: 自機方向を中心にした扇状弾。口の前の輪は、弧の数を弾の数に合わせ(1 本の弧が 1 発の弾)、大きくする
class SpreadShotAttack final : public IBossAttackPattern {
public:
    // count: 弾の数, spreadAngleDeg: 弾と弾の間の角度(度), speed: 弾速
    SpreadShotAttack(int count, float spreadAngleDeg, float speed)
        : count_(count), spreadAngleDeg_(spreadAngleDeg), speed_(speed) {}

    void Fire(IBossActions& actions) const override;
    BossMuzzleRingStyle GetMuzzleRingStyle() const override;

private:
    int count_;            // 弾の数
    float spreadAngleDeg_; // 弾と弾の間の角度(度)
    float speed_;          // 弾速
};
