#include "AimedShootComponent.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../Bullet/BulletFactory.h"
#include "EnemyComponent.h"
#include "EnemyShootUtil.h"
#include "TelegraphComponent.h"
#include "Method.h"
#include <algorithm>
#include <cmath>

namespace {
// JSON のキー名
constexpr const char* kKeyShootInterval = "shootInterval";
constexpr const char* kKeyFirstShotDelay = "firstShotDelay";
constexpr const char* kKeyBulletSpeed = "bulletSpeed";
constexpr const char* kKeyAimError = "aimErrorDegrees";
constexpr const char* kKeyRecover = "recoverAfterBulletGone";
constexpr const char* kKeyScreenMargin = "screenMargin";

// 自機との距離が、これ未満なら撃たない(方向が定まらないため)
constexpr float kMinAimDistance = 0.001f;
// 度からラジアンへの変換に使う
constexpr float kPi = 3.14159265358979f;
constexpr float kHalfTurnDegrees = 180.0f;
constexpr float kTwoPi = 6.28318530717959f;
// 狙いのばらつきの基準にする、世界の上方向(これと狙いの向きが平行に近いときは、代わりの軸を使う)
const Vector3 kWorldUp = { 0.0f, 1.0f, 0.0f };
const Vector3 kWorldRight = { 1.0f, 0.0f, 0.0f };
constexpr float kParallelThreshold = 0.99f;
// 敵ごとに乱数の種を変えるための、増やしていく番号(同じ並びの繰り返しを避けつつ、毎回の実行で同じになる)
unsigned int g_nextSeed = 1;
} // namespace

AimedShootComponent::AimedShootComponent() : random_(g_nextSeed++) {}

bool AimedShootComponent::IsLastBulletAlive_() const {
    const auto bullet = lastBullet_.lock();
    return bullet && bullet->IsActive();
}

Vector3 AimedShootComponent::ApplyAimError_(const Vector3& dir) {
    if (aimErrorDegrees_ <= 0.0f) return dir;

    // dir に直交する2本の軸 u, v を作り、その平面上でランダムにずらす。ずらす大きさの上限は tan(許容角)
    const Vector3 reference = (std::fabs(Dot(dir, kWorldUp)) > kParallelThreshold) ? kWorldRight : kWorldUp;
    const Vector3 u = Normalize(Cross(reference, dir));
    const Vector3 v = Cross(dir, u);

    // 円の中でばらつかせる(角が2倍近くに伸びないように、半径は sqrt で面積に対して一様にする)
    const float maxOffset = std::tan(aimErrorDegrees_ * kPi / kHalfTurnDegrees);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float radius = maxOffset * std::sqrt(unit(random_));
    const float angle = unit(random_) * kTwoPi;
    const float ou = radius * std::cos(angle);
    const float ov = radius * std::sin(angle);
    return Normalize({ dir.x + u.x * ou + v.x * ov,
                       dir.y + u.y * ou + v.y * ov,
                       dir.z + u.z * ou + v.z * ov });
}

void AimedShootComponent::Update(float deltaTime) {
    if (!owner_) return;

    // 撃破されて消えていく途中の敵は撃たない(EnemyComponent が死亡状態のとき)
    if (const auto* enemy = owner_->GetComponent<EnemyComponent>()) {
        if (enemy->IsDead()) return;
    }

    auto* scene = BaseScene::GetActiveScene();
    if (!scene) return;

    // 画面外にいる間は、タイマーを進めない(入ってから firstShotDelay_ 待つ。出ている間に弾がたまらない)
    const Vector3 myPos = owner_->GetTransform().translate;
    if (!EnemyShootUtil::IsInsideScreen(*scene, myPos, screenMargin_)) return;

    // ---- タイマーの更新 ----
    visibleTime_ += deltaTime;
    sinceLastShot_ += deltaTime;
    if (IsLastBulletAlive_()) {
        sinceBulletGone_ = 0.0f;
    } else {
        sinceBulletGone_ += deltaTime;
    }

    // ---- 予備動作(体の光) ----
    // 次に撃てるまでの秒数(最初の 1 発は、待ちの残り。以降は、間隔の残りと、弾が消えてからの間の残りの、長いほう)。
    // 撃つ時刻の手前から光り、撃つ時刻に終わる。予備動作の最中は撃たない
    auto* telegraph = owner_->GetComponent<TelegraphComponent>();
    if (telegraph) {
        float secondsUntilFire = firstShotDelay_ - visibleTime_;
        if (hasShot_) {
            secondsUntilFire = shootInterval_ - sinceLastShot_;
            if (!IsLastBulletAlive_()) {
                secondsUntilFire = std::max(secondsUntilFire, recoverAfterBulletGone_ - sinceBulletGone_);
            }
        }
        if (telegraph->HoldFire(secondsUntilFire)) return;
    }

    // ---- 発射してよいか ----
    if (!hasShot_) {
        if (visibleTime_ < firstShotDelay_) return;
    } else {
        if (sinceLastShot_ < shootInterval_) return;
        // 前の弾が消えた(撃ち落とされた)直後は、少し間を空ける。弾がまだ飛んでいる間は、この条件は見ない
        if (!IsLastBulletAlive_() && sinceBulletGone_ < recoverAfterBulletGone_) return;
    }

    // ---- 自機を狙う ----
    const auto player = EnemyShootUtil::FindPlayer(*scene);
    if (!player) return;
    const Vector3 playerPos = player->GetTransform().translate;
    const Vector3 toPlayer = { playerPos.x - myPos.x, playerPos.y - myPos.y, playerPos.z - myPos.z };
    if (Length(toPlayer) < kMinAimDistance) return;
    const Vector3 aimDir = ApplyAimError_(Normalize(toPlayer));

    // ---- 発射 ----
    BulletSpec spec;
    spec.position = myPos;
    spec.velocity = { aimDir.x * bulletSpeed_, aimDir.y * bulletSpeed_, aimDir.z * bulletSpeed_ };
    auto bullet = BulletFactory::CreateEnemy(spec);
    lastBullet_ = bullet;
    scene->AddRootObject(bullet);

    if (telegraph) telegraph->End();
    hasShot_ = true;
    sinceLastShot_ = 0.0f;
    sinceBulletGone_ = 0.0f;
}

void AimedShootComponent::Serialize(nlohmann::json& j) const {
    j[kKeyShootInterval] = shootInterval_;
    j[kKeyFirstShotDelay] = firstShotDelay_;
    j[kKeyBulletSpeed] = bulletSpeed_;
    j[kKeyAimError] = aimErrorDegrees_;
    j[kKeyRecover] = recoverAfterBulletGone_;
    j[kKeyScreenMargin] = screenMargin_;
}

void AimedShootComponent::Deserialize(const nlohmann::json& j) {
    shootInterval_ = j.value(kKeyShootInterval, shootInterval_);
    firstShotDelay_ = j.value(kKeyFirstShotDelay, firstShotDelay_);
    bulletSpeed_ = j.value(kKeyBulletSpeed, bulletSpeed_);
    aimErrorDegrees_ = j.value(kKeyAimError, aimErrorDegrees_);
    recoverAfterBulletGone_ = j.value(kKeyRecover, recoverAfterBulletGone_);
    screenMargin_ = j.value(kKeyScreenMargin, screenMargin_);
}
