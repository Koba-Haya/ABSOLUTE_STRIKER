#pragma once
#include "IChargeState.h"

// 突進する敵の状態: 予備動作。TelegraphComponent の duration の間、自機と反対へ少し引きながら(だんだん遅くなる)、体が光って震える。
// 引く動きは、この状態が位置を足して行う(StraightMoveComponent などの、位置を足す動きと競合しない)。
// 当たり判定は、予備動作の間も普通のまま(自機の弾で倒せる。自機に当たれば自滅する)。
// 次へ進む条件: 予備動作の時間が過ぎた -> ChargeAimState(狙う。引いて距離が開いていても、そのまま突進する)
class ChargeWindupState : public IChargeState {
public:
    // awayDir: 引く向き(予備動作の始めの、自機から離れる向き。長さ 1)
    explicit ChargeWindupState(const Vector3& awayDir) : awayDir_(awayDir) {}

    // 予備動作の光り方を始める
    void Enter(const ChargeStateContext& context) override;
    std::unique_ptr<IChargeState> Update(const ChargeStateContext& context) override;

private:
    Vector3 awayDir_;
    float timer_ = 0.0f; // 予備動作の経過時間
};
