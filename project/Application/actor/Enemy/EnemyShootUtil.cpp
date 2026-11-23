#include "EnemyShootUtil.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../Player/PlayerComponent.h"
#include "AbsoluteEngine/scene/IScreenView.h"
#include "Application/occlusion/OcclusionWorld.h"

namespace EnemyShootUtil {

std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer(const BaseScene& scene) {
    for (const auto& obj : scene.GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            return obj;
        }
    }
    return nullptr;
}

bool IsHiddenByIsland(const AbsoluteEngine::IScreenView& screen, const Vector3& position) {
    if (!screen.HasCamera()) return false;
    return OcclusionWorld::GetInstance().IsSegmentBlocked(screen.GetEye(), position, OccluderKind::kIsland);
}

void IslandHideGate::Update(const AbsoluteEngine::IScreenView& screen, const Vector3& position, float deltaTime) {
    if (IsHiddenByIsland(screen, position)) {
        visibleTime_ = 0.0f; // 隠れた。出てきたら、待ち時間を最初から数える
    } else {
        visibleTime_ += deltaTime;
    }
}

} // namespace EnemyShootUtil
