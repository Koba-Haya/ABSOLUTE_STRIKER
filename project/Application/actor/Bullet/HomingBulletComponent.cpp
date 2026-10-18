#include "HomingBulletComponent.h"
#include "../Enemy/EnemyComponent.h"
#include "../Enemy/BossComponent.h"
#include "RibbonTrailComponent.h"
#include <cmath>

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------
void HomingBulletComponent::Initialize(
    const Vector3& p0, const Vector3& p1, float duration,
    std::weak_ptr<AbsoluteEngine::GameObject> target)
{
    startPos_   = p0;       // P0: 始点（固定）
    controlPos_ = p1;       // P1: 制御点（固定）
    duration_   = duration; // 着弾までの総時間
    progress_   = 0.0f;     // 進行度リセット
    target_     = target;   // 追尾対象
    isActive_   = true;

    // 前フレーム座標を始点で初期化しておく（初回の差分計算に備える）
    prevPos_ = p0;
}

// -----------------------------------------------------------------------
// Update（毎フレーム呼ばれる）
// -----------------------------------------------------------------------
void HomingBulletComponent::Update(float deltaTime) {
    if (!owner_ || !isActive_) return;

    // --- ターゲットの生存確認（weak_ptrをlock()して安全にアクセス） ---
    auto targetShared = target_.lock();
    if (targetShared) {
        auto enemyComp = targetShared->GetComponent<EnemyComponent>();
        auto bossComp  = targetShared->GetComponent<BossComponent>();

        bool alive = false;
        if (enemyComp && !enemyComp->IsDead()) {
            alive = true;
        } else if (bossComp && bossComp->GetHp() > 0) {
            alive = true;
        }

        if (!alive) {
            // ターゲットが消滅 → 弾も消す
            Finish_();
            return;
        }
    } else {
        // ターゲットのGameObjectが既に解放済み → 弾も消す
        Finish_();
        return;
    }

    // --- 進行度 t の更新 ---
    progress_ += deltaTime / duration_;
    if (progress_ > 1.0f) {
        progress_ = 1.0f; // 1.0を超えないようにクランプ
    }

    // --- P2 (終点) をターゲットの最新座標から毎フレーム取得 ---
    Vector3 p2 = targetShared->GetTransform().translate;

    // --- 2次ベジェ曲線の公式で現在座標を計算 ---
    // B(t) = (1-t)^2 * P0 + 2*(1-t)*t * P1 + t^2 * P2
    float t  = progress_;
    float t1 = 1.0f - t;

    float coeff0 = t1 * t1;          // (1-t)^2
    float coeff1 = 2.0f * t1 * t;    // 2*(1-t)*t
    float coeff2 = t * t;             // t^2

    Vector3 newPos = {
        coeff0 * startPos_.x   + coeff1 * controlPos_.x + coeff2 * p2.x,
        coeff0 * startPos_.y   + coeff1 * controlPos_.y + coeff2 * p2.y,
        coeff0 * startPos_.z   + coeff1 * controlPos_.z + coeff2 * p2.z
    };

    // --- 進行方向への回転（前フレーム座標との差分ベクトルで算出） ---
    auto& trans = owner_->GetTransform();
    Vector3 diff = {
        newPos.x - prevPos_.x,
        newPos.y - prevPos_.y,
        newPos.z - prevPos_.z
    };
    float diffLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

    if (diffLen > 0.0001f) {
        // XZ平面のヨー角（弾の横方向の向き）
        float yaw   = std::atan2(diff.x, diff.z);
        // XZ投影長からピッチ角（弾の上下方向の向き）
        float vxzLen = std::sqrt(diff.x * diff.x + diff.z * diff.z);
        float pitch  = std::atan2(-diff.y, vxzLen);

        trans.rotate = { pitch, yaw, 0.0f };
    }

    // --- 座標をTransformに直接適用 ---
    trans.translate = newPos;

    // 次フレームの差分計算のために現在座標を保存
    prevPos_ = newPos;

    // --- t = 1.0 に到達したら敵に命中したとみなして消滅 ---
    /*if (progress_ >= 1.0f) {
        isActive_ = false;
        owner_->Destroy();
    }*/
}

// -----------------------------------------------------------------------
// OnCollision（当たり判定は生かしておく）
// -----------------------------------------------------------------------
void HomingBulletComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_) return;

    // タグのみで判定する（名前に「Enemy」が含まれる敵弾を誤って消滅させるバグを防ぐ）
    const std::string& otherTag = other->GetTag();
    if (otherTag == "Enemy" || otherTag == "Boss") {
        Finish_();
    }
}

// -----------------------------------------------------------------------
// Finish_（弾が消えるときの共通処理）
// -----------------------------------------------------------------------
void HomingBulletComponent::Finish_() {
    isActive_ = false;

    // 尾を弾から切り離して残す。弾と一緒に尾が一瞬で消えて、光の帯がぶつ切りに見えるのを防ぐ
    if (auto* trail = owner_->GetComponent<RibbonTrailComponent>()) {
        trail->DetachToScene();
    }

    owner_->Destroy();
}
