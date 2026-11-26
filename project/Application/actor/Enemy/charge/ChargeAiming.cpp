#include "IChargeState.h"
#include "../../Player/PlayerFinder.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include <cmath>

namespace ChargeAiming {

bool FindPlayer(const ChargeStateContext& context, Vector3& playerPos, float& dist) {
    const auto playerObj = PlayerFinder::FindPlayer(context.query);
    if (!playerObj) return false;
    const Vector3 currentPos = context.owner.GetTransform().translate;
    playerPos = playerObj->GetTransform().translate;
    const Vector3 toPlayer = { playerPos.x - currentPos.x, playerPos.y - currentPos.y, playerPos.z - currentPos.z };
    dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    return true;
}

Vector3 ComputeDirection(const Vector3& currentPos, const Vector3& playerPos, float dist, const Vector3& cameraVelocity, float chargeSpeed) {
    // 自機は、カメラの目の位置を基準に毎フレーム置かれるので、操作していなくても、カメラのレール前進と同じ速さで流れ続ける。
    // そのぶんを先読みして、向きを一度だけ決める(以後は追わない = 自機の回避の操作には追従しない)
    const float timeToImpact = (chargeSpeed > kMinDistance) ? (dist / chargeSpeed) : 0.0f;
    const Vector3 predictedPos = {
        playerPos.x + cameraVelocity.x * timeToImpact,
        playerPos.y + cameraVelocity.y * timeToImpact,
        playerPos.z + cameraVelocity.z * timeToImpact
    };
    const Vector3 toPredicted = {
        predictedPos.x - currentPos.x,
        predictedPos.y - currentPos.y,
        predictedPos.z - currentPos.z
    };
    const float predDist = std::sqrt(toPredicted.x * toPredicted.x + toPredicted.y * toPredicted.y + toPredicted.z * toPredicted.z);
    if (predDist > kMinDistance) {
        return { toPredicted.x / predDist, toPredicted.y / predDist, toPredicted.z / predDist };
    }
    return { (playerPos.x - currentPos.x) / dist, (playerPos.y - currentPos.y) / dist, (playerPos.z - currentPos.z) / dist };
}

} // namespace ChargeAiming
