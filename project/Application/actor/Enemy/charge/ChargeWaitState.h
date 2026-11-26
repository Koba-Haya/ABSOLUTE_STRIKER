#pragma once
#include "IChargeState.h"

// 突進する敵の状態: 接近待ち(最初の状態)。この部品は何もしない(StraightMoveComponent など、別の移動の部品に動きを任せる)。
// 自機との距離が triggerRange 以下になったら、予備動作の部品(TelegraphComponent)があれば予備動作へ、なければ、その場で狙いを決めて突進へ。
class ChargeWaitState : public IChargeState {
public:
    std::unique_ptr<IChargeState> Update(const ChargeStateContext& context) override;
};
