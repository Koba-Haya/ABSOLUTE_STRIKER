#pragma once
#include <vector>
#include <memory>

#include "../Type/Vector.h"

namespace AbsoluteEngine {

class GameObject;
class ColliderComponent;

// 衝突判定と、めり込みの押し戻しを行うクラス(状態を持たない)。
// 以前は Singleton(GetInstance)だったが、使うのは SceneWorld だけなので、SceneWorld のメンバとして持たせた。
class CollisionManager {
public:
    // オブジェクトの一覧の中の、衝突する物同士を調べて、めり込みを押し戻す
    void Update(const std::vector<std::shared_ptr<GameObject>>& rootObjects);

private:
    struct CollisionResult {
        bool isHit = false;
        Vector3 normal{0, 0, 0};   // BからAに向かう法線ベクトル (Aを押し出す方向)
        float depth = 0.0f;        // めり込みの深さ
    };

    void CollectColliders(std::shared_ptr<GameObject> obj, std::vector<ColliderComponent*>& outColliders);
    CollisionResult CheckCollision(ColliderComponent* a, ColliderComponent* b);
    void ResolvePenetration(ColliderComponent* a, ColliderComponent* b, const CollisionResult& result);
};

} // namespace AbsoluteEngine
