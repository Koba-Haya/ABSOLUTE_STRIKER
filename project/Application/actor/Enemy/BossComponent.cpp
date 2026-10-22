#include "BossComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "../Bullet/BulletFactory.h"
#include "../Player/PlayerComponent.h"
#include "GameCamera.h"
#include <cmath>

namespace {
// フェーズごとの攻撃間隔・弾速（マジックナンバーをここに集約し、調整しやすくする）
constexpr float kAimedIntervalPhase1 = 1.6f;
constexpr float kAimedIntervalPhase2 = 1.3f;
constexpr float kAimedIntervalPhase3 = 0.8f;
constexpr float kAimedSpeed = 24.0f;

constexpr float kSpreadIntervalPhase2 = 2.2f;
constexpr float kSpreadIntervalPhase3 = 1.5f;
constexpr float kSpreadSpeed = 16.0f;
constexpr int   kSpreadCountPhase2 = 5;
constexpr int   kSpreadCountPhase3 = 8;
constexpr float kSpreadAngleDeg = 12.0f;

constexpr float kSpinSpeed = 0.6f; // rad/sec

// カメラの前方に留まる距離・左右上下の揺れ幅と速さ
// 基準はカメラ(=レール上の位置)であり、自機の動きには影響されない。
// 従来(自機の前方14)と見た目の位置が変わらないよう、自機のカメラ前方距離(22)+14 = 36 にしている
constexpr float kHoverDistanceFromCamera = 36.0f;
constexpr float kWeaveAmplitudeX = 6.0f;  // 左右の振れ幅
constexpr float kWeaveAmplitudeY = 3.0f;  // 上下の振れ幅
constexpr float kWeaveSpeedX = 0.5f;      // rad/sec
constexpr float kWeaveSpeedY = 0.7f;      // rad/sec（Xと素数っぽい比率にして周期がズレるようにする）

constexpr float kHitFlashDuration = 0.12f; // 被弾フラッシュの継続時間（秒）
}

void BossComponent::Update(float deltaTime) {
    if (!isActive_ || !owner_) return;

    // 完全に静止した箱に見えないよう、常時ゆっくり自転させる（演出コスト最小のリアクション）
    spinTimer_ += deltaTime * kSpinSpeed;
    owner_->GetTransform().rotate.y = spinTimer_;

    // 被弾フラッシュの経過処理
    if (hitFlashTimer_ > 0.0f) {
        hitFlashTimer_ -= deltaTime;
        if (hitFlashTimer_ <= 0.0f) {
            hitFlashTimer_ = 0.0f;
            if (auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
                modelComp->SetColor(colorBeforeFlash_);
            }
        }
    }

    UpdateMovement(deltaTime);
    UpdatePhase();
    UpdateAttack(deltaTime);
}

std::shared_ptr<AbsoluteEngine::GameObject> BossComponent::FindPlayer() const {
    auto scene = BaseScene::GetActiveScene();
    if (!scene) return nullptr;

    for (const auto& obj : scene->GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            return obj;
        }
    }
    return nullptr;
}

void BossComponent::UpdateMovement(float deltaTime) {
    auto scene = BaseScene::GetActiveScene();
    if (!scene) return;
    auto* camera = scene->GetMainCamera();
    if (!camera) return;

    weaveTimer_ += deltaTime;

    // 位置の基準はカメラの目の位置。カメラはレールに沿って動くので、ボスもレールに追従する
    const Vector3 basePos = camera->GetEye();
    const Vector3 forward = camera->GetForward();
    const Vector3 right    = camera->GetRight();
    const Vector3 up       = camera->GetActualUp();

    const float weaveX = std::sin(weaveTimer_ * kWeaveSpeedX) * kWeaveAmplitudeX;
    const float weaveY = std::sin(weaveTimer_ * kWeaveSpeedY) * kWeaveAmplitudeY;

    owner_->GetTransform().translate = {
        basePos.x + forward.x * kHoverDistanceFromCamera + right.x * weaveX + up.x * weaveY,
        basePos.y + forward.y * kHoverDistanceFromCamera + right.y * weaveX + up.y * weaveY,
        basePos.z + forward.z * kHoverDistanceFromCamera + right.z * weaveX + up.z * weaveY
    };
}

void BossComponent::UpdatePhase() {
    if (maxHp_ <= 0) return;
    const float hpRatio = static_cast<float>(hp_) / static_cast<float>(maxHp_);
    if (hpRatio <= 1.0f / 3.0f) {
        phase_ = BossPhase::Phase3;
    } else if (hpRatio <= 2.0f / 3.0f) {
        phase_ = BossPhase::Phase2;
    } else {
        phase_ = BossPhase::Phase1;
    }
}

void BossComponent::UpdateAttack(float deltaTime) {
    aimedShotTimer_ += deltaTime;
    spreadShotTimer_ += deltaTime;

    float aimedInterval = kAimedIntervalPhase1;
    switch (phase_) {
        case BossPhase::Phase1: aimedInterval = kAimedIntervalPhase1; break;
        case BossPhase::Phase2: aimedInterval = kAimedIntervalPhase2; break;
        case BossPhase::Phase3: aimedInterval = kAimedIntervalPhase3; break;
    }
    if (aimedShotTimer_ >= aimedInterval) {
        aimedShotTimer_ -= aimedInterval;
        FireAimedShot(kAimedSpeed);
    }

    // 扇状弾はPhase2以降のみ（Phase1は単発弾のみで動きを覚えてもらう）
    if (phase_ == BossPhase::Phase2 || phase_ == BossPhase::Phase3) {
        const float spreadInterval = (phase_ == BossPhase::Phase3) ? kSpreadIntervalPhase3 : kSpreadIntervalPhase2;
        if (spreadShotTimer_ >= spreadInterval) {
            spreadShotTimer_ -= spreadInterval;
            const int count = (phase_ == BossPhase::Phase3) ? kSpreadCountPhase3 : kSpreadCountPhase2;
            FireSpreadShot(count, kSpreadAngleDeg, kSpreadSpeed);
        }
    }
}

namespace {
// 発射位置(spawnPos)から速度(vel)の直進弾を1発生成してアクティブシーンに追加する
void SpawnBossBullet(const Vector3& spawnPos, const Vector3& vel) {
    auto scene = BaseScene::GetActiveScene();
    if (!scene) return;

    BulletSpec spec;
    spec.name     = "BossBullet";
    spec.position = spawnPos;
    spec.velocity = vel;
    spec.color    = Vector4{ 1.0f, 0.2f, 0.6f, 1.0f }; // ボス弾は他の敵弾と区別できるよう色を変える
    scene->AddRootObject(BulletFactory::CreateEnemy(spec));
}
} // namespace

void BossComponent::FireAimedShot(float speed) {
    if (!owner_) return;
    auto playerObj = FindPlayer();
    if (!playerObj) return;

    const Vector3 spawnPos = owner_->GetTransform().translate;
    Vector3 toPlayer = {
        playerObj->GetTransform().translate.x - spawnPos.x,
        playerObj->GetTransform().translate.y - spawnPos.y,
        playerObj->GetTransform().translate.z - spawnPos.z
    };
    const float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (len < 0.001f) return;
    toPlayer.x /= len; toPlayer.y /= len; toPlayer.z /= len;

    SpawnBossBullet(spawnPos, { toPlayer.x * speed, toPlayer.y * speed, toPlayer.z * speed });
}

void BossComponent::FireSpreadShot(int count, float spreadAngleDeg, float speed) {
    if (!owner_ || count <= 0) return;
    auto playerObj = FindPlayer();
    if (!playerObj) return;

    const Vector3 spawnPos = owner_->GetTransform().translate;
    Vector3 toPlayer = {
        playerObj->GetTransform().translate.x - spawnPos.x,
        playerObj->GetTransform().translate.y - spawnPos.y,
        playerObj->GetTransform().translate.z - spawnPos.z
    };
    const float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (len < 0.001f) return;
    toPlayer.x /= len; toPlayer.y /= len; toPlayer.z /= len;

    // プレイヤーは主にXY平面（画面平面）上を動くため、Z軸周りに回転させて扇状に振り分ける。
    // 中心(0発分)から見て左右対称になるよう、-half〜+halfの角度で等間隔に配置する。
    const float spreadRad = spreadAngleDeg * 3.14159265f / 180.0f;
    const float half = static_cast<float>(count - 1) * 0.5f;
    for (int i = 0; i < count; ++i) {
        const float angle = (static_cast<float>(i) - half) * spreadRad;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        Vector3 dir = {
            toPlayer.x * c - toPlayer.y * s,
            toPlayer.x * s + toPlayer.y * c,
            toPlayer.z
        };
        SpawnBossBullet(spawnPos, { dir.x * speed, dir.y * speed, dir.z * speed });
    }
}

void BossComponent::TakeDamage(int damage) {
    if (!isActive_) return;

    hp_ -= damage;
    if (hp_ <= 0) {
        hp_ = 0;
        isActive_ = false; // 撃破
        if (owner_) {
            owner_->Destroy();
        }
    } else if (owner_) {
        // 即死しなかった場合の被弾リアクション
        if (auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
            if (hitFlashTimer_ <= 0.0f) {
                colorBeforeFlash_ = modelComp->GetColor();
            }
            modelComp->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
            hitFlashTimer_ = kHitFlashDuration;
        }
    }
}

void BossComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_ || !other) return;

    if (other->GetName().find("PlayerBullet") != std::string::npos || other->GetTag() == "PlayerBullet") {
        TakeDamage(1);
    }
}
