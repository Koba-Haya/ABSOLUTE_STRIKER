#include "ChargeWaitState.h"
#include "ChargeRunState.h"
#include "ChargeWindupState.h"
#include "AbsoluteEngine/scene/GameObject.h"

std::unique_ptr<IChargeState> ChargeWaitState::Update(const ChargeStateContext& context) {
    Vector3 playerPos{ 0.0f, 0.0f, 0.0f };
    float dist = 0.0f;
    if (!ChargeAiming::FindPlayer(context, playerPos, dist)) return nullptr;
    if (dist > context.settings.triggerRange || dist < ChargeAiming::kMinDistance) return nullptr;

    const Vector3 currentPos = context.owner.GetTransform().translate;
    if (context.telegraph) {
        // 予備動作: 自機から離れる向きへ引く(向きは、範囲に入った瞬間に決める)
        const Vector3 awayDir = { (currentPos.x - playerPos.x) / dist, (currentPos.y - playerPos.y) / dist, (currentPos.z - playerPos.z) / dist };
        return std::make_unique<ChargeWindupState>(awayDir);
    }
    // 予備動作の部品がないときは、その場で狙いを決めて、突進する
    return std::make_unique<ChargeRunState>(
        ChargeAiming::ComputeDirection(currentPos, playerPos, dist, context.cameraVelocity, context.settings.chargeSpeed));
}
