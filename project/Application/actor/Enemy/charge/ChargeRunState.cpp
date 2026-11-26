#include "ChargeRunState.h"
#include "AbsoluteEngine/scene/GameObject.h"

std::unique_ptr<IChargeState> ChargeRunState::Update(const ChargeStateContext& context) {
    const float step = context.settings.chargeSpeed * context.deltaTime;
    context.owner.Translate({ direction_.x * step, direction_.y * step, direction_.z * step });
    return nullptr;
}
