#include "CollisionManager.h"
#include "GameObject.h"
#include "ColliderComponent.h"
#include "Component.h"
#include "RigidbodyComponent.h"
#include "Transform.h"
#include <cmath>
#include <algorithm>

namespace AbsoluteEngine {

void CollisionManager::CollectColliders(std::shared_ptr<GameObject> obj, std::vector<ColliderComponent*>& outColliders) {
    if (!obj || !obj->IsActive()) return;

    auto collider = obj->GetComponent<ColliderComponent>();
    // 形状を持つ(種類が None でない)ものだけを集める
    if (collider && collider->GetShape()) {
        outColliders.push_back(collider);
    }

    for (auto& child : obj->GetChildren()) {
        CollectColliders(child, outColliders);
    }
}

void CollisionManager::Update(const std::vector<std::shared_ptr<GameObject>>& rootObjects) {
    std::vector<ColliderComponent*> colliders;
    for (const auto& obj : rootObjects) {
        CollectColliders(obj, colliders);
    }

    for (size_t i = 0; i < colliders.size(); ++i) {
        for (size_t j = i + 1; j < colliders.size(); ++j) {
            ColliderComponent* a = colliders[i];
            ColliderComponent* b = colliders[j];

            CollisionResult result = CheckCollision(*a, *b);
            if (result.isHit) {
                // めり込み解決
                ResolvePenetration(a, b, result);

                // 各コンポーネントのOnCollisionを呼び出す
                for (const auto& comp : a->GetOwner()->GetComponents()) {
                    comp->OnCollision(b->GetOwner());
                }
                for (const auto& comp : b->GetOwner()->GetComponents()) {
                    comp->OnCollision(a->GetOwner());
                }
            }
        }
    }
}

CollisionResult CollisionManager::CheckCollision(const ColliderComponent& a, const ColliderComponent& b) const {
    // 形の組み合わせ(球と球、球と箱、箱と箱)の選び分けは、形状のクラスの Double Dispatch に任せる。
    // ここは、形の種類を知らない(新しい形を足しても、この関数は変わらない)
    return a.GetShape()->Collide(a.GetCollisionPlacement(), *b.GetShape(), b.GetCollisionPlacement());
}

void CollisionManager::ResolvePenetration(ColliderComponent* a, ColliderComponent* b, const CollisionResult& result) {
    auto rbA = a->GetOwner()->GetComponent<RigidbodyComponent>();
    auto rbB = b->GetOwner()->GetComponent<RigidbodyComponent>();

    bool aMove = (rbA != nullptr) && !rbA->IsKinematic();
    bool bMove = (rbB != nullptr) && !rbB->IsKinematic();

    if (!aMove && !bMove) return;

    if (aMove && bMove) {
        // 両方動く場合は質量に応じて押し出し量を分配するのが理想だが、今回は簡易的に半分ずつ
        const float halfDepth = result.depth * 0.5f;
        a->GetOwner()->Translate({result.normal.x * halfDepth, result.normal.y * halfDepth, result.normal.z * halfDepth});
        b->GetOwner()->Translate({-result.normal.x * halfDepth, -result.normal.y * halfDepth, -result.normal.z * halfDepth});

        // 反発処理 (簡易的: 速度をゼロまたは反転)
        Vector3 vA = rbA->GetVelocity();
        Vector3 vB = rbB->GetVelocity();
        rbA->SetVelocity({vA.x * 0.5f, vA.y * 0.5f, vA.z * 0.5f});
        rbB->SetVelocity({vB.x * 0.5f, vB.y * 0.5f, vB.z * 0.5f});
    } else if (aMove) {
        a->GetOwner()->Translate({result.normal.x * result.depth, result.normal.y * result.depth, result.normal.z * result.depth});
        
        // Bは動かない壁のようなものなので、Aの速度を減衰させる (跳ね返りは未実装、または簡易的に)
        Vector3 vA = rbA->GetVelocity();
        // 法線方向の速度成分を打ち消す
        float dot = vA.x * result.normal.x + vA.y * result.normal.y + vA.z * result.normal.z;
        if (dot < 0) {
            vA.x -= dot * result.normal.x;
            vA.y -= dot * result.normal.y;
            vA.z -= dot * result.normal.z;
            rbA->SetVelocity(vA);
        }
    } else if (bMove) {
        b->GetOwner()->Translate({-result.normal.x * result.depth, -result.normal.y * result.depth, -result.normal.z * result.depth});

        Vector3 vB = rbB->GetVelocity();
        float dot = vB.x * -result.normal.x + vB.y * -result.normal.y + vB.z * -result.normal.z;
        if (dot < 0) {
            vB.x -= dot * -result.normal.x;
            vB.y -= dot * -result.normal.y;
            vB.z -= dot * -result.normal.z;
            rbB->SetVelocity(vB);
        }
    }
}

} // namespace AbsoluteEngine
