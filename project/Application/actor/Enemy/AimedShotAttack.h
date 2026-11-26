#pragma once
#include "IBossAttackPattern.h"

// 攻撃パターン: 自機狙いの単発弾。口の前の輪は標準のまま
class AimedShotAttack final : public IBossAttackPattern {
public:
    // speed: 弾速
    explicit AimedShotAttack(float speed) : speed_(speed) {}

    void Fire(IBossActions& actions) const override;

private:
    float speed_; // 弾速
};
