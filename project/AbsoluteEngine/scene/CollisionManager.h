#pragma once
#include <vector>
#include <memory>

#include "../Type/Vector.h"
#include "collision/CollisionShape.h"

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
    void CollectColliders(std::shared_ptr<GameObject> obj, std::vector<ColliderComponent*>& outColliders);
    // a と b の衝突を、形状のクラスに調べさせる(結果の法線は、a を押し出す向き)
    CollisionResult CheckCollision(const ColliderComponent& a, const ColliderComponent& b) const;
    void ResolvePenetration(ColliderComponent* a, ColliderComponent* b, const CollisionResult& result);
};

} // namespace AbsoluteEngine
