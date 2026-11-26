#pragma once
#include "ITelegraphGlow.h"

// 光り方: 体全体の色を、光る色に替える(鳥の突進、横切りの敵など。モデルが立方体の敵)。
// 光る色は赤みにして、被弾の白いフラッシュ(EnemyComponent)と区別する
class BodyTintGlow final : public ITelegraphGlow {
public:
    // tint: 予備動作の間、体にかける色
    explicit BodyTintGlow(const Vector4& tint) : tint_(tint) {}

    void OnBegin(AbsoluteEngine::GameObject& owner, float progress) override;
    void OnEnd(AbsoluteEngine::GameObject* owner, const Vector4& baseColor) override;

private:
    Vector4 tint_; // 予備動作の間、体にかける色
};
