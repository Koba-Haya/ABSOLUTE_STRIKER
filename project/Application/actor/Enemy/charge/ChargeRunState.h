#pragma once
#include "IChargeState.h"

// 突進する敵の状態: 突進(最後の状態)。狙いを決めた瞬間の向きへ、直進し続ける。
// 向きは毎フレーム計算し直さない(計算し直すと、目標の近くで行き過ぎては戻る振動が起きるため)。次の状態は無い。
class ChargeRunState : public IChargeState {
public:
    // direction: 突進の向き(長さ 1)
    explicit ChargeRunState(const Vector3& direction) : direction_(direction) {}

    std::unique_ptr<IChargeState> Update(const ChargeStateContext& context) override;

    // 決めた向きへ進むだけなので、カメラは使わない(カメラが無いフレームも進む)
    bool UsesCamera() const override { return false; }

private:
    Vector3 direction_;
};
