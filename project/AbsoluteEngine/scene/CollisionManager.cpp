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
    if (collider && collider->GetSettings().type != ColliderComponent::Type::None) {
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

            CollisionResult result = CheckCollision(a, b);
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

CollisionManager::CollisionResult CollisionManager::CheckCollision(ColliderComponent* a, ColliderComponent* b) {
    CollisionResult result;
    result.isHit = false;

    Vector3 posA = a->GetOwner()->GetTransform().translate;
    posA.x += a->GetSettings().centerOffset.x; posA.y += a->GetSettings().centerOffset.y; posA.z += a->GetSettings().centerOffset.z;
    Vector3 posB = b->GetOwner()->GetTransform().translate;
    posB.x += b->GetSettings().centerOffset.x; posB.y += b->GetSettings().centerOffset.y; posB.z += b->GetSettings().centerOffset.z;

    if (a->GetSettings().type == ColliderComponent::Type::Sphere && b->GetSettings().type == ColliderComponent::Type::Sphere) {
        float dx = posA.x - posB.x;
        float dy = posA.y - posB.y;
        float dz = posA.z - posB.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        float rSum = a->GetSettings().radius + b->GetSettings().radius;
        
        if (distSq <= rSum * rSum) {
            result.isHit = true;
            float dist = std::sqrt(distSq);
            if (dist > 0.0001f) {
                result.normal = {dx / dist, dy / dist, dz / dist};
                result.depth = rSum - dist;
            } else {
                result.normal = {0, 1, 0};
                result.depth = rSum;
            }
        }
        return result;
    }
    
    if (a->GetSettings().type == ColliderComponent::Type::AABB && b->GetSettings().type == ColliderComponent::Type::AABB) {
        float dx = posA.x - posB.x;
        float dy = posA.y - posB.y;
        float dz = posA.z - posB.z;
        
        float penX = (a->GetSettings().size.x + b->GetSettings().size.x) - std::abs(dx);
        float penY = (a->GetSettings().size.y + b->GetSettings().size.y) - std::abs(dy);
        float penZ = (a->GetSettings().size.z + b->GetSettings().size.z) - std::abs(dz);
        
        if (penX > 0 && penY > 0 && penZ > 0) {
            result.isHit = true;
            if (penX <= penY && penX <= penZ) {
                result.depth = penX;
                result.normal = {dx > 0 ? 1.0f : -1.0f, 0, 0};
            } else if (penY <= penX && penY <= penZ) {
                result.depth = penY;
                result.normal = {0, dy > 0 ? 1.0f : -1.0f, 0};
            } else {
                result.depth = penZ;
                result.normal = {0, 0, dz > 0 ? 1.0f : -1.0f};
            }
        }
        return result;
    }

    // Sphere vs AABB
    ColliderComponent* sphere = a->GetSettings().type == ColliderComponent::Type::Sphere ? a : b;
    ColliderComponent* aabb = a->GetSettings().type == ColliderComponent::Type::AABB ? a : b;
    if (sphere->GetSettings().type == ColliderComponent::Type::Sphere && aabb->GetSettings().type == ColliderComponent::Type::AABB) {
        Vector3 posS = sphere->GetOwner()->GetTransform().translate;
        posS.x += sphere->GetSettings().centerOffset.x; posS.y += sphere->GetSettings().centerOffset.y; posS.z += sphere->GetSettings().centerOffset.z;
        Vector3 posBox = aabb->GetOwner()->GetTransform().translate;
        posBox.x += aabb->GetSettings().centerOffset.x; posBox.y += aabb->GetSettings().centerOffset.y; posBox.z += aabb->GetSettings().centerOffset.z;
        
        float closestX = std::max(posBox.x - aabb->GetSettings().size.x, std::min(posS.x, posBox.x + aabb->GetSettings().size.x));
        float closestY = std::max(posBox.y - aabb->GetSettings().size.y, std::min(posS.y, posBox.y + aabb->GetSettings().size.y));
        float closestZ = std::max(posBox.z - aabb->GetSettings().size.z, std::min(posS.z, posBox.z + aabb->GetSettings().size.z));

        float dx = posS.x - closestX;
        float dy = posS.y - closestY;
        float dz = posS.z - closestZ;
        float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq <= sphere->GetSettings().radius * sphere->GetSettings().radius) {
            result.isHit = true;
            float dist = std::sqrt(distSq);
            
            if (dist > 0.0001f) {
                result.normal = {dx / dist, dy / dist, dz / dist};
                result.depth = sphere->GetSettings().radius - dist;
            } else {
                // 球の中心がAABBの内部にある場合 (中心から一番近い面へ押し出す)
                float dpx = posS.x - posBox.x;
                float dpy = posS.y - posBox.y;
                float dpz = posS.z - posBox.z;
                float px = aabb->GetSettings().size.x - std::abs(dpx);
                float py = aabb->GetSettings().size.y - std::abs(dpy);
                float pz = aabb->GetSettings().size.z - std::abs(dpz);
                
                if (px <= py && px <= pz) {
                    result.depth = sphere->GetSettings().radius + px;
                    result.normal = {dpx > 0 ? 1.0f : -1.0f, 0, 0};
                } else if (py <= px && py <= pz) {
                    result.depth = sphere->GetSettings().radius + py;
                    result.normal = {0, dpy > 0 ? 1.0f : -1.0f, 0};
                } else {
                    result.depth = sphere->GetSettings().radius + pz;
                    result.normal = {0, 0, dpz > 0 ? 1.0f : -1.0f};
                }
            }
            
            // aabb が b の場合、法線は BからAへのベクトルなので AがSphereの場合はそのまま
            // もし AがAABBの場合、法線の向きを逆にする必要がある
            if (a->GetSettings().type == ColliderComponent::Type::AABB) {
                result.normal.x = -result.normal.x;
                result.normal.y = -result.normal.y;
                result.normal.z = -result.normal.z;
            }
        }
        return result;
    }

    return result;
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
