#include "BulletComponent.h"

#include "AbsoluteEngine/scene/BaseScene.h"
#include "../Enemy/EnemyComponent.h"
#include "../Enemy/BossComponent.h"
#include "Application/actor/Player/PlayerComponent.h"
#include "Application/occlusion/OcclusionWorld.h"

void BulletComponent::Initialize(const Vector3 &vel) {
  velocity_ = vel;
  isActive_ = true;
  lifeTimer_ = 0.0f;
}

void BulletComponent::Update(float deltaTime) {
  if (!isActive_ || !GetOwner()) return;

  lifeTimer_ += deltaTime;
  if (lifeTimer_ >= maxLife_) {
    isActive_ = false;
    GetOwner()->Destroy(); // 寿命が来たらオブジェクトごと消滅
    return;
  }

  const Vector3 previousPos = GetOwner()->GetTransform().translate;
  GetOwner()->Translate({velocity_.x * deltaTime, velocity_.y * deltaTime, velocity_.z * deltaTime});

  // 今フレームの移動の線分が、島に遮られたら消える(自機の弾も敵弾も、島を貫通しない)。
  // 点ではなく線分で調べるので、速い弾が 1 フレームで島を飛び越えることもない。
  // 雲は含めない(雲の裏へは、通常弾は届く)。移動のあとで調べるので、島の内側で終わった弾も、ここで消える
  if (OcclusionWorld::GetInstance().IsSegmentBlocked(previousPos, GetOwner()->GetTransform().translate,OccluderKind::kIsland)) {
    isActive_ = false;
    GetOwner()->Destroy();
  }
}

namespace {
// 弾のタグ(GameObject::GetTag の値)
constexpr const char* kEnemyBulletTag  = "EnemyBullet";
constexpr const char* kPlayerBulletTag = "PlayerBullet";
} // namespace

void BulletComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_ || !GetOwner()) return;

    // タグのみで判定する（名前検索は「EnemyBullet」に「Enemy」が含まれるなど誤反応を招くため廃止）
    const std::string& ownerTag = GetOwner()->GetTag();
    const std::string& otherTag = other->GetTag();

    // 相手が「通常弾」か。ロックオン弾(ホーミング)も同じ PlayerBullet タグだが、BulletComponent を持たない。
    // そのため BulletComponent を持つ PlayerBullet = 通常弾、と区別できる(ホーミング弾は敵弾と相殺しない)
    const auto* otherBullet = other->GetComponent<BulletComponent>();

    if (ownerTag == kEnemyBulletTag) {
        if (otherTag == "Player") {
            // 敵弾 → プレイヤー本体に当たって消える
            isActive_ = false;
            GetOwner()->Destroy();
        } else if (otherTag == kPlayerBulletTag && otherBullet && canBeShotDown_) {
            // 敵弾 → 通常弾に撃ち落とされる(撃ち落とせない弾は、そのまま飛び続ける)
            isActive_ = false;
            GetOwner()->Destroy();
        }
    } else if (ownerTag == kPlayerBulletTag) {
        if (otherTag == "Enemy" || otherTag == "Boss") {
            // プレイヤー弾 → 敵・ボス本体に当たって消える
            isActive_ = false;
            GetOwner()->Destroy();
        } else if (otherTag == kEnemyBulletTag && otherBullet) {
            // 通常弾 → 敵弾に当たって消える。撃ち落とせない弾でも、通常弾のほうは消える。
            // 敵弾側も自分の OnCollision で同じ判定をするので、両方が自分自身だけを消す(二重処理にならない)
            isActive_ = false;
            GetOwner()->Destroy();
        }
    }
}
