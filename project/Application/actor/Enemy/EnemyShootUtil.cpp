#include "EnemyShootUtil.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../Player/PlayerComponent.h"
#include "GameCamera.h"
#include "Application/occlusion/OcclusionWorld.h"

namespace {
// 画面内判定で、カメラ背後(クリップ空間の w が 0 以下)とみなす下限。既存の射撃コンポーネントと同じ値
constexpr float kMinClipW = 0.01f;
} // namespace

namespace EnemyShootUtil {

bool IsInsideScreen(const BaseScene& scene, const Vector3& position, float ndcMargin) {
    const auto* camera = scene.GetMainCamera();
    if (!camera) return false;

    const Matrix4x4 vpMat = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());

    // 世界座標をクリップ空間に変換する(行ベクトル x 行列。w は奥行きに相当する)
    const float w = position.x * vpMat.m[0][3] + position.y * vpMat.m[1][3]
                  + position.z * vpMat.m[2][3] + vpMat.m[3][3];
    if (w <= kMinClipW) return false; // カメラの背後

    const float ndcX = (position.x * vpMat.m[0][0] + position.y * vpMat.m[1][0]
                      + position.z * vpMat.m[2][0] + vpMat.m[3][0]) / w;
    const float ndcY = (position.x * vpMat.m[0][1] + position.y * vpMat.m[1][1]
                      + position.z * vpMat.m[2][1] + vpMat.m[3][1]) / w;

    return ndcX >= -ndcMargin && ndcX <= ndcMargin && ndcY >= -ndcMargin && ndcY <= ndcMargin;
}

std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer(const BaseScene& scene) {
    for (const auto& obj : scene.GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            return obj;
        }
    }
    return nullptr;
}

bool IsHiddenByIsland(const Vector3& position) {
    const auto* scene = BaseScene::GetActiveScene();
    const auto* camera = scene ? scene->GetMainCamera() : nullptr;
    if (!camera) return false;
    return OcclusionWorld::GetInstance().IsSegmentBlocked(camera->GetEye(), position, OccluderKind::kIsland);
}

void IslandHideGate::Update(const Vector3& position, float deltaTime) {
    if (IsHiddenByIsland(position)) {
        visibleTime_ = 0.0f; // 隠れた。出てきたら、待ち時間を最初から数える
    } else {
        visibleTime_ += deltaTime;
    }
}

} // namespace EnemyShootUtil
