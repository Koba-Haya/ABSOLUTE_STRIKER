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
    if (!GetOwner()) return;

    // 被弾フラッシュの経過処理（生きている間の被弾リアクション）
    if (hitFlashTimer_ > 0.0f) {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f) {
            hitFlashTimer_ = 0.0f;
            if (auto* modelComp = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>()) {
                modelComp->SetColor(colorBeforeFlash_);
            }
        }
    }

    if (isDead_) {
        // -----------------------------------------------------------------------
        // 倒れた直後の 1 回だけ、受け取る側の全員へ知らせる。
        // Update は毎フレーム呼ばれるため、effectFired_ フラグで 1 回に制限する。
        // -----------------------------------------------------------------------
        if (!effectFired_ && !observers_.empty()) {
            effectFired_ = true;
            const EnemyDefeatEvent event{ *GetOwner(), GetOwner()->GetTransform().translate, destroyReason_ };
            // 受け取る側が、知らせの中で登録を外しても壊れないよう、一覧の写しに対して呼ぶ
            const std::vector<IEnemyDefeatObserver*> observers = observers_;
            for (IEnemyDefeatObserver* observer : observers) {
                observer->OnEnemyDefeated(event);
            }
        }

        // ディゾルブアニメーションを進める
        dissolveTimer_ += deltaTime;
        float t = std::clamp(dissolveTimer_ / dissolveDuration_, 0.0f, 1.0f);

        if (auto dissolveComp = GetOwner()->GetComponent<AbsoluteEngine::DissolveComponent>()) {
            // 設定のコピーを編集して、SetSettings で戻す
            AbsoluteEngine::DissolveComponent::Settings dissolve = dissolveComp->GetSettings();
            dissolve.enable = true;
            dissolve.threshold = t;
            dissolve.edgeRange = 0.05f;
            dissolve.edgeColor = { 1.0f, 0.3f, 0.0f }; // オレンジのエッジ
            dissolve.maskColor  = { 1.0f, 0.1f, 0.0f };
            dissolveComp->SetSettings(dissolve);

            if (t >= 1.0f) {
                GetOwner()->Destroy();
            }
        } else {
            // DissolveComponent がない場合は即時消滅
            GetOwner()->Destroy();
        }
    }
}

bool EnemyComponent::AddObserver(IEnemyDefeatObserver* observer) {
    if (!observer) return false;
    if (std::find(observers_.begin(), observers_.end(), observer) != observers_.end()) return false;
    observers_.push_back(observer);
    return true;
}

void EnemyComponent::RemoveObserver(IEnemyDefeatObserver* observer) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

void EnemyComponent::OnHit(int damage, EnemyDestroyReason reason) {
    if (isDead_) return;

    hp_ -= damage;
    if (hp_ <= 0) {
        destroyReason_ = reason;
        isDead_    = true;
        isActive_  = false;
        dissolveTimer_ = 0.0f;
        // コールバックは Update 内で 1 回だけ発火する（タイミング保証のため）
    } else if (GetOwner()) {
        // 即死しなかった場合の被弾リアクション（タレット等、複数発耐える敵向け）
        if (auto* modelComp = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>()) {
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
    if (other->GetTag() == "PlayerBullet" && HomingBulletComponent::CanHit(*other, GetOwner())) {
        OnHit();
    }
}
