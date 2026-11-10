#include "BulletComponent.h"

#include "AbsoluteEngine/scene/BaseScene.h"
#include "../Enemy/EnemyComponent.h"
#include "../Enemy/BossComponent.h"
#include "Application/actor/Player/PlayerComponent.h"

void BulletComponent::Initialize(const Vector3 &vel) {
  velocity_ = vel;
  isActive_ = true;
  lifeTimer_ = 0.0f;
}

void BulletComponent::Update(float deltaTime) {
  if (!isActive_ || !owner_) return;

  lifeTimer_ += deltaTime;
  if (lifeTimer_ >= maxLife_) {
    isActive_ = false;
    owner_->Destroy(); // 寿命が来たらオブジェクトごと消滅
    return;
  }

  auto& t = owner_->GetTransform();
  t.translate.x += velocity_.x * deltaTime;
  t.translate.y += velocity_.y * deltaTime;
  t.translate.z += velocity_.z * deltaTime;
}

namespace {
// 弾のタグ(GameObject::GetTag の値)
constexpr const char* kEnemyBulletTag  = "EnemyBullet";
constexpr const char* kPlayerBulletTag = "PlayerBullet";
} // namespace

void BulletComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_ || !owner_) return;

    // タグのみで判定する（名前検索は「EnemyBullet」に「Enemy」が含まれるなど誤反応を招くため廃止）
    const std::string& ownerTag = owner_->GetTag();
    const std::string& otherTag = other->GetTag();

    // 相手が「通常弾」か。ロックオン弾(ホーミング)も同じ PlayerBullet タグだが、BulletComponent を持たない。
    // そのため BulletComponent を持つ PlayerBullet = 通常弾、と区別できる(ホーミング弾は敵弾と相殺しない)
    const auto* otherBullet = other->GetComponent<BulletComponent>();

    if (ownerTag == kEnemyBulletTag) {
        if (otherTag == "Player") {
            // 敵弾 → プレイヤー本体に当たって消える
            isActive_ = false;
            owner_->Destroy();
        } else if (otherTag == kPlayerBulletTag && otherBullet && canBeShotDown_) {
            // 敵弾 → 通常弾に撃ち落とされる(撃ち落とせない弾は、そのまま飛び続ける)
            isActive_ = false;
            owner_->Destroy();
        }
    } else if (ownerTag == kPlayerBulletTag) {
        if (otherTag == "Enemy" || otherTag == "Boss") {
            // プレイヤー弾 → 敵・ボス本体に当たって消える
            isActive_ = false;
            owner_->Destroy();
        } else if (otherTag == kEnemyBulletTag && otherBullet) {
            // 通常弾 → 敵弾に当たって消える。撃ち落とせない弾でも、通常弾のほうは消える。
            // 敵弾側も自分の OnCollision で同じ判定をするので、両方が自分自身だけを消す(二重処理にならない)
            isActive_ = false;
            owner_->Destroy();
        }
    }
}
