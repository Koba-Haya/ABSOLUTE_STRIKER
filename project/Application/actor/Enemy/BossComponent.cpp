#include "BossComponent.h"
#include "BossModelSpec.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "AbsoluteEngine/scene/IScreenView.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "../Bullet/BulletFactory.h"
#include "../Player/PlayerFinder.h"
#include "../Bullet/HomingBulletComponent.h"
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

// カメラの前方に留まる距離・左右上下の揺れ幅と速さ
// 基準はカメラ(=レール上の位置)であり、自機の動きには影響されない。
// 従来(自機の前方14)と見た目の位置が変わらないよう、自機のカメラ前方距離(22)+14 = 36 にしている
constexpr float kHoverDistanceFromCamera = 36.0f;
constexpr float kWeaveAmplitudeX = 6.0f;  // 左右の振れ幅
constexpr float kWeaveAmplitudeY = 3.0f;  // 上下の振れ幅
constexpr float kWeaveSpeedX = 0.5f;      // rad/sec
constexpr float kWeaveSpeedY = 0.7f;      // rad/sec（Xと素数っぽい比率にして周期がズレるようにする）

// ボスのモデルの正面を、カメラの方へ向けるための補正(ラジアン)。実機で、正面が奥を向いていたため、半回転(180 度)させる
constexpr float kModelFrontYawOffset = 3.14159265f;

// アニメーションの切り替えの混ぜ合わせ時間(秒)。急に切り替わって見えないようにする
constexpr float kHoverBlendSeconds = 0.25f;  // 待機へ戻る
constexpr float kAttackBlendSeconds = 0.15f; // 攻撃の予告へ入る
constexpr float kHitBlendSeconds = 0.05f;    // 被弾のよろめきへ入る(すぐに反応して見えるよう短く)
// 単発弾を撃つ、この時間(秒)前から、攻撃の予告(あごを開く、翼を開くなど)を始める
constexpr float kAttackLeadSeconds = 0.6f;
}

void BossComponent::Update(float deltaTime) {
    if (!isActive_ || !GetOwner()) return;

    UpdateMovement(deltaTime);
    UpdatePhase();
    UpdateAttack(deltaTime);
    UpdateAnimation();
}

void BossComponent::UpdateAnimation() {
    auto* model = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>();
    if (!model) return;

    // 被弾: すぐによろめく(攻撃の予告の途中でも割り込む)。連射を受けたときも、毎回頭から再生し直す(restart)
    if (hitRequested_) {
        hitRequested_ = false;
        attackRequested_ = false;
        model->PlayAnimation(BossAnimation::kHit, false, kHitBlendSeconds, true);
        animState_ = AnimState::Hit;
        return;
    }

    // 攻撃の予告: よろめいている間は始めない(Hit が終わってから、次の発射の予告を出す)
    if (attackRequested_ && animState_ != AnimState::Hit) {
        attackRequested_ = false;
        model->PlayAnimation(BossAnimation::kAttack, false, kAttackBlendSeconds, false);
        animState_ = AnimState::Attack;
        return;
    }
    attackRequested_ = false;

    // 最初の 1 回(None)と、Hit / Attack が最後まで再生されたあとは、待機へ戻る
    if (animState_ == AnimState::None || (animState_ != AnimState::Hover && model->IsAnimationFinished())) {
        model->PlayAnimation(BossAnimation::kHover, true, kHoverBlendSeconds, false);
        animState_ = AnimState::Hover;
    }
}

std::shared_ptr<AbsoluteEngine::GameObject> BossComponent::FindPlayer() const {
    return PlayerFinder::FindPlayer(query_);
}

void BossComponent::UpdateMovement(float deltaTime) {
    if (!screen_.HasCamera()) return;

    weaveTimer_ += deltaTime;

    // 位置の基準はカメラの目の位置。カメラはレールに沿って動くので、ボスもレールに追従する
    const Vector3 basePos = screen_.GetEye();
    const Vector3 forward = screen_.GetForward();
    const Vector3 right    = screen_.GetRight();
    const Vector3 up       = screen_.GetActualUp();

    const float weaveX = std::sin(weaveTimer_ * kWeaveSpeedX) * kWeaveAmplitudeX;
    const float weaveY = std::sin(weaveTimer_ * kWeaveSpeedY) * kWeaveAmplitudeY;

    GetOwner()->GetTransform().translate = {
        basePos.x + forward.x * kHoverDistanceFromCamera + right.x * weaveX + up.x * weaveY,
        basePos.y + forward.y * kHoverDistanceFromCamera + right.y * weaveX + up.y * weaveY,
        basePos.z + forward.z * kHoverDistanceFromCamera + right.z * weaveX + up.z * weaveY
    };

    // 正面(モデルの +Z)を、カメラの方へ向ける。自機は、モデルの +Z を進行方向へ向けて飛ぶ(PlayerComponent と同じ規約)。
    // ボスは、その自機と向かい合うので、進行方向の逆(カメラの向きの逆)へ向く。左右(Y 軸)の向きだけを合わせる
    // (ボスのモデルは、自機のモデルと、書き出しの前後の向きが逆なので、kModelFrontYawOffset だけ回して合わせる)
    GetOwner()->GetTransform().rotate = { 0.0f, std::atan2(-forward.x, -forward.z) + kModelFrontYawOffset, 0.0f };
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
    // 発射の少し前に、攻撃の予告のアニメーションを始める(1 回の発射につき 1 回)
    if (!attackAnnounced_ && aimedShotTimer_ >= aimedInterval - kAttackLeadSeconds) {
        attackAnnounced_ = true;
        attackRequested_ = true;
    }
    if (aimedShotTimer_ >= aimedInterval) {
        aimedShotTimer_ -= aimedInterval;
        attackAnnounced_ = false;
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
// 発射位置(spawnPos)から速度(vel)の直進弾を1発生成して、窓口(spawner)からシーンへ出す
void SpawnBossBullet(AbsoluteEngine::ISpawner& spawner, const Vector3& spawnPos, const Vector3& vel) {
    BulletSpec spec;
    spec.name     = "BossBullet";
    spec.position = spawnPos;
    spec.velocity = vel;
    spec.color    = Vector4{ 1.0f, 0.2f, 0.6f, 1.0f }; // ボス弾は他の敵弾と区別できるよう色を変える
    spawner.Spawn(BulletFactory::CreateEnemy(spec));
}
} // namespace

void BossComponent::FireAimedShot(float speed) {
    if (!GetOwner()) return;
    auto playerObj = FindPlayer();
    if (!playerObj) return;

    const Vector3 spawnPos = GetOwner()->GetTransform().translate;
    Vector3 toPlayer = {
        playerObj->GetTransform().translate.x - spawnPos.x,
        playerObj->GetTransform().translate.y - spawnPos.y,
        playerObj->GetTransform().translate.z - spawnPos.z
    };
    const float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (len < 0.001f) return;
    toPlayer.x /= len; toPlayer.y /= len; toPlayer.z /= len;

    SpawnBossBullet(spawner_, spawnPos, { toPlayer.x * speed, toPlayer.y * speed, toPlayer.z * speed });
}

void BossComponent::FireSpreadShot(int count, float spreadAngleDeg, float speed) {
    if (!GetOwner() || count <= 0) return;
    auto playerObj = FindPlayer();
    if (!playerObj) return;

    const Vector3 spawnPos = GetOwner()->GetTransform().translate;
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
        SpawnBossBullet(spawner_, spawnPos, { dir.x * speed, dir.y * speed, dir.z * speed });
    }
}

void BossComponent::TakeDamage(int damage) {
    if (!isActive_) return;

    hp_ -= damage;
    if (hp_ <= 0) {
        hp_ = 0;
        // 撃破。オブジェクトはここでは消さない(撃破演出がボスの姿を使うため。消えるのはシーンの終わり)。
        // isActive_ が false になるので、以降は、移動・攻撃・被弾の処理が止まる。撃破の検知は IsActive() で行う
        isActive_ = false;
    } else {
        // 即死しなかった場合の被弾リアクション(よろめきのアニメーション。次の UpdateAnimation で再生する)
        hitRequested_ = true;
    }
}

void BossComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_ || !other) return;

    // ロックオン弾(ホーミング弾)は、ロック対象にしか当たらない
    if ((other->GetName().find("PlayerBullet") != std::string::npos || other->GetTag() == "PlayerBullet") &&
        HomingBulletComponent::CanHit(*other, GetOwner())) {
        TakeDamage(1);
    }
}
