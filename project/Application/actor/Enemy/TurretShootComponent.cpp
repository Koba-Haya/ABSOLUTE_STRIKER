#include "TurretShootComponent.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../../actor/Bullet/BulletFactory.h"
#include "../../actor/Player/PlayerComponent.h"
#include "GameCamera.h"
#include "TelegraphComponent.h"
#include "externals/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>

namespace {
// JSON のキー名
constexpr const char* kKeyShootInterval = "shootInterval";
constexpr const char* kKeyBurstCount = "burstCount";
constexpr const char* kKeyBurstDelay = "burstDelay";
constexpr const char* kKeyBulletSpeed = "bulletSpeed";
constexpr const char* kKeyFireRange = "fireRange";
constexpr const char* kKeyLeadAim = "leadAim";

// 自機の速度の推定のなめらかさ(0 から 1。大きいほど、新しい値を速く取り入れる)
constexpr float kVelocitySmoothing = 0.2f;
// 自機の速度の推定に使う 1 フレームの時間の下限(0 での割り算を避ける)
constexpr float kMinDeltaTime = 0.0001f;
// 2 次方程式の a(= v.v - s^2)が、これ以上(0 に近い)なら、弾が自機より速くないので、解なしとする
constexpr float kMinSpeedMargin = 0.0001f;
} // namespace

void TurretShootComponent::Update(float deltaTime) {
    if (!owner_) return;

    TrackPlayerVelocity(deltaTime);

    if (burstShotsRemaining_ > 0) {
        // バースト中: burstDelay_間隔で残弾を消化する（クールダウンタイマーは進めない）
        burstTimer_ += deltaTime;
        if (burstTimer_ >= burstDelay_) {
            burstTimer_ -= burstDelay_;
            Vector3 dir;
            if (CanFire(dir)) {
                FireOneBullet(dir);
            }
            --burstShotsRemaining_;
        }
        return;
    }

    cooldownTimer_ += deltaTime;

    // 予備動作(口の光): 撃てる状態(画面内、射程内)のときだけ。撃つ時刻の手前から光り、撃つ時刻に終わる。
    // 予備動作の最中は撃たない。撃てない状態になったら、光を消す
    Vector3 dir;
    const bool canFireNow = CanFire(dir);
    auto* telegraph = owner_->GetComponent<TelegraphComponent>();
    if (telegraph) {
        if (!canFireNow) {
            telegraph->End();
        } else if (telegraph->HoldFire(shootInterval_ - cooldownTimer_)) {
            return;
        }
    }

    if (cooldownTimer_ < shootInterval_) return;

    // クールダウン完了。画面内にいてプレイヤーが見つかった瞬間にバーストを開始する
    if (!canFireNow) return;

    cooldownTimer_ = 0.0f;
    if (telegraph) telegraph->End();
    FireOneBullet(dir);
    burstShotsRemaining_ = burstCount_ - 1;
    burstTimer_ = 0.0f;
}

bool TurretShootComponent::CanFire(Vector3& outToPlayerDir) const {
    if (!owner_) return false;

    auto scene = BaseScene::GetActiveScene();
    if (!scene) return false;

    auto* camera = scene->GetMainCamera();
    if (!camera) return false;

    const Vector3 currentPos = owner_->GetTransform().translate;

    // --- 画面内チェック（NDC ±1.2 マージン内にいる場合のみ発射を許可）---
    Matrix4x4 viewMat = camera->GetViewMatrix();
    Matrix4x4 projMat = camera->GetProjectionMatrix();
    Matrix4x4 vpMat   = Multiply(viewMat, projMat);

    float w = currentPos.x * vpMat.m[0][3] + currentPos.y * vpMat.m[1][3]
            + currentPos.z * vpMat.m[2][3] + vpMat.m[3][3];
    if (w <= 0.01f) return false; // カメラの背後にいる場合は発射しない

    float ndcX = (currentPos.x * vpMat.m[0][0] + currentPos.y * vpMat.m[1][0]
               + currentPos.z * vpMat.m[2][0] + vpMat.m[3][0]) / w;
    float ndcY = (currentPos.x * vpMat.m[0][1] + currentPos.y * vpMat.m[1][1]
               + currentPos.z * vpMat.m[2][1] + vpMat.m[3][1]) / w;

    constexpr float ndcMargin = 1.2f;
    if (ndcX < -ndcMargin || ndcX > ndcMargin || ndcY < -ndcMargin || ndcY > ndcMargin) return false;

    // --- プレイヤーを探す ---
    std::shared_ptr<AbsoluteEngine::GameObject> playerObj = nullptr;
    for (const auto& obj : scene->GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            playerObj = obj;
            break;
        }
    }
    if (!playerObj) return false;

    // --- プレイヤー方向（タレットは非移動のため、進行方向ではなく毎回直接この方向を狙う）---
    Vector3 playerPos = playerObj->GetTransform().translate;
    Vector3 toPlayer = {
        playerPos.x - currentPos.x,
        playerPos.y - currentPos.y,
        playerPos.z - currentPos.z
    };
    float toPlayerLen = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (toPlayerLen < 0.001f) return false;

    outToPlayerDir = { toPlayer.x / toPlayerLen, toPlayer.y / toPlayerLen, toPlayer.z / toPlayerLen };

    // --- 遠すぎるときは撃たない(弾が届く前に自機が通り過ぎるのを避ける) ---
    if (fireRange_ > 0.0f && toPlayerLen > fireRange_) return false;

    // --- 自機がもう通り過ぎて後方にいる場合は撃たない ---
    // タレットは移動しないため進行方向(facingDir_)を持てないが、レールの進行方向＝カメラの
    // 前方向と一致するため、それを基準に「自機への方向が進行方向寄りかどうか」で判定する。
    // 自機が接近中〜横に並ぶ間はtoPlayerが進行方向の逆〜直角寄りになり、
    // 通り過ぎると進行方向寄り（内積が大きく正）に転じる。
    Vector3 forward = camera->GetForward();
    const float towardBehind = outToPlayerDir.x * forward.x + outToPlayerDir.y * forward.y + outToPlayerDir.z * forward.z;
    constexpr float kMaxForwardAlignment = 0.3f;
    if (towardBehind > kMaxForwardAlignment) return false;

    // --- 自機の動く先を読む(通り過ぎる判定のあと。判定は、今の自機の方向で行う) ---
    if (leadAim_) {
        Vector3 leadDir;
        if (CalcLeadDirection(currentPos, playerPos, leadDir)) {
            outToPlayerDir = leadDir;
        }
    }

    return true;
}

void TurretShootComponent::TrackPlayerVelocity(float deltaTime) {
    auto scene = BaseScene::GetActiveScene();
    if (!scene) return;
    std::shared_ptr<AbsoluteEngine::GameObject> playerObj;
    for (const auto& obj : scene->GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            playerObj = obj;
            break;
        }
    }
    if (!playerObj) return;

    const Vector3 pos = playerObj->GetTransform().translate;
    if (hasPrevPlayerPos_ && deltaTime > kMinDeltaTime) {
        const Vector3 raw = { (pos.x - prevPlayerPos_.x) / deltaTime, (pos.y - prevPlayerPos_.y) / deltaTime,
                              (pos.z - prevPlayerPos_.z) / deltaTime };
        playerVelocity_.x += (raw.x - playerVelocity_.x) * kVelocitySmoothing;
        playerVelocity_.y += (raw.y - playerVelocity_.y) * kVelocitySmoothing;
        playerVelocity_.z += (raw.z - playerVelocity_.z) * kVelocitySmoothing;
    }
    prevPlayerPos_ = pos;
    hasPrevPlayerPos_ = true;
}

bool TurretShootComponent::CalcLeadDirection(const Vector3& turretPos, const Vector3& playerPos, Vector3& outDir) const {
    // 弾が時刻 t に、自機の予測位置(playerPos + v * t)に着く条件: |r + v * t| = s * t
    // (r = 敵から自機へのベクトル、v = 自機の速度、s = 弾の速さ)。両辺を 2 乗して、t の 2 次方程式にする:
    //   (v.v - s^2) t^2 + 2 (r.v) t + r.r = 0
    const Vector3 r = { playerPos.x - turretPos.x, playerPos.y - turretPos.y, playerPos.z - turretPos.z };
    const Vector3& v = playerVelocity_;
    const float a = (v.x * v.x + v.y * v.y + v.z * v.z) - bulletSpeed_ * bulletSpeed_;
    const float b = 2.0f * (r.x * v.x + r.y * v.y + r.z * v.z);
    const float c = r.x * r.x + r.y * r.y + r.z * r.z;
    // 弾が自機より速くない(a が 0 に近い、または正)、または判別式が負のときは、追いつけないので、解なし
    if (a >= -kMinSpeedMargin) return false;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return false;
    // a < 0 なので、正の解は (-b - sqrt(D)) / (2a)
    const float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (t <= 0.0f) return false;

    const Vector3 aimPoint = { r.x + v.x * t, r.y + v.y * t, r.z + v.z * t };
    const float len = std::sqrt(aimPoint.x * aimPoint.x + aimPoint.y * aimPoint.y + aimPoint.z * aimPoint.z);
    if (len < 0.001f) return false;
    outDir = { aimPoint.x / len, aimPoint.y / len, aimPoint.z / len };
    return true;
}

void TurretShootComponent::FireOneBullet(const Vector3& dir) {
    auto scene = BaseScene::GetActiveScene();
    if (!scene || !owner_) return;

    BulletSpec spec;
    spec.position = owner_->GetTransform().translate;
    spec.velocity = { dir.x * bulletSpeed_, dir.y * bulletSpeed_, dir.z * bulletSpeed_ };
    scene->AddRootObject(BulletFactory::CreateEnemy(spec));
}

void TurretShootComponent::Serialize(nlohmann::json& j) const {
    j[kKeyShootInterval] = shootInterval_;
    j[kKeyBurstCount] = burstCount_;
    j[kKeyBurstDelay] = burstDelay_;
    j[kKeyBulletSpeed] = bulletSpeed_;
    j[kKeyFireRange] = fireRange_;
    j[kKeyLeadAim] = leadAim_;
}

void TurretShootComponent::Deserialize(const nlohmann::json& j) {
    shootInterval_ = j.value(kKeyShootInterval, shootInterval_);
    burstCount_ = std::max(1, j.value(kKeyBurstCount, burstCount_));
    burstDelay_ = j.value(kKeyBurstDelay, burstDelay_);
    bulletSpeed_ = j.value(kKeyBulletSpeed, bulletSpeed_);
    fireRange_ = j.value(kKeyFireRange, fireRange_);
    leadAim_ = j.value(kKeyLeadAim, leadAim_);
}
