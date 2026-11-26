#include "ChargeAimState.h"
#include "ChargeRunState.h"
#include "AbsoluteEngine/scene/GameObject.h"

std::unique_ptr<IChargeState> ChargeAimState::Update(const ChargeStateContext& context) {
    Vector3 playerPos{ 0.0f, 0.0f, 0.0f };
    float dist = 0.0f;
    if (!ChargeAiming::FindPlayer(context, playerPos, dist)) return nullptr;
    if (dist < ChargeAiming::kMinDistance) return nullptr;
    return std::make_unique<ChargeRunState>(ChargeAiming::ComputeDirection(context.owner.GetTransform().translate, playerPos, dist,
                                                                           context.cameraVelocity, context.settings.chargeSpeed));
}
