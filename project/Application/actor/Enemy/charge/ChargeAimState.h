#pragma once
#include "IChargeState.h"

// 突進する敵の状態: 狙う(予備動作のあと)。自機が見つかり次第、距離に関係なく、突進の向きを一度だけ決める
// (予備動作で引いて、距離が範囲より開いていても、突進をやめない)。
// 次へ進む条件: 自機が見つかった -> ChargeRunState(突進)
class ChargeAimState : public IChargeState {
public:
    std::unique_ptr<IChargeState> Update(const ChargeStateContext& context) override;
};
