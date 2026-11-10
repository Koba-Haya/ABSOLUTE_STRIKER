#include "EnemyComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/DissolveComponent.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "../Bullet/HomingBulletComponent.h"
#include <algorithm>

namespace {
constexpr float kHitFlashDuration = 0.12f; // 被弾フラッシュの継続時間（秒）
}

void EnemyComponent::Update(float deltaTime) {
    if (!owner_) return;

    // 被弾フラッシュの経過処理（生きている間の被弾リアクション）
    if (hitFlashTimer_ > 0.0f) {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f) {
            hitFlashTimer_ = 0.0f;
            if (auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
                modelComp->SetColor(colorBeforeFlash_);
            }
        }
    }

    if (isDead_) {
        // -----------------------------------------------------------------------
        // 撃破直後の1回だけ onDestroyed コールバックを発火する。
        // Update は毎フレーム呼ばれるため、effectFired_ フラグで1回に制限する。
        // -----------------------------------------------------------------------
        if (!effectFired_ && onDestroyed) {
            effectFired_ = true;
            onDestroyed(owner_->GetTransform().translate);
        }

        // ディゾルブアニメーションを進める
        dissolveTimer_ += deltaTime;
        float t = std::clamp(dissolveTimer_ / dissolveDuration_, 0.0f, 1.0f);

        if (auto dissolveComp = owner_->GetComponent<AbsoluteEngine::DissolveComponent>()) {
            auto& dissolve = *dissolveComp;
            dissolve.enable = true;
            dissolve.threshold = t;
            dissolve.edgeRange = 0.05f;
            dissolve.edgeColor = { 1.0f, 0.3f, 0.0f }; // オレンジのエッジ
            dissolve.maskColor  = { 1.0f, 0.1f, 0.0f };

            if (t >= 1.0f) {
                owner_->Destroy();
            }
        } else {
            // DissolveComponent がない場合は即時消滅
            owner_->Destroy();
        }
    }
}

void EnemyComponent::OnHit(int damage) {
    if (isDead_) return;

    hp_ -= damage;
    if (hp_ <= 0) {
        isDead_    = true;
        isActive_  = false;
        dissolveTimer_ = 0.0f;
        // コールバックは Update 内で 1 回だけ発火する（タイミング保証のため）
    } else if (owner_) {
        // 即死しなかった場合の被弾リアクション（タレット等、複数発耐える敵向け）
        if (auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
            if (hitFlashTimer_ <= 0.0f) {
                colorBeforeFlash_ = modelComp->GetColor(); // 多重被弾で元の色を上書きしないよう最初の1回だけ保存
            }
            modelComp->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
            hitFlashTimer_ = kHitFlashDuration;
        }
    }
}

void EnemyComponent::Serialize(nlohmann::json& j) const {
    j["maxHp"] = maxHp_;
}

void EnemyComponent::Deserialize(const nlohmann::json& j) {
    maxHp_ = j.value("maxHp", 1);
    hp_ = maxHp_; // ロード時は常に満タンから開始する
}

void EnemyComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_ || isDead_) return;

    // タグ判定：自機弾が当たったらダメージを受ける
    // ロックオン弾(ホーミング弾)は、ロック対象にしか当たらない。ほかの敵は通り抜けさせる
    if (other->GetTag() == "PlayerBullet" && HomingBulletComponent::CanHit(*other, owner_)) {
        OnHit();
    }
}
