#include "BossOrbEffects.h"
#include "AbsoluteEngine/graphics/particle/ParticleManager.h"
#include "BossEffectParticles.h"
#include "Method.h"
#include <cmath>
#include <random>

namespace {
using namespace BossEffectParticles;

constexpr float kTwoPi = 6.28318530718f;

// ===== 発射 =====
// 中心の閃光(柔らかい光)。板の一辺(ワールド): 始め -> 終わり
constexpr float kLaunchFlashSizeStart = 1.6f;
constexpr float kLaunchFlashSizeEnd = 5.0f;
constexpr float kLaunchFlashLife = 0.14f;
inline const Vector4 kLaunchFlashColor{ 0.95f, 0.88f, 1.00f, 0.95f };
// 星形の閃光(十字の光)
constexpr float kLaunchStarSizeStart = 4.5f;
constexpr float kLaunchStarSizeEnd = 8.5f;
constexpr float kLaunchStarLife = 0.24f;
inline const Vector4 kLaunchStarColor{ 0.95f, 0.88f, 1.00f, 1.0f };
// 進む向きへ伸びる光の筋(細長い板。長さと太さ: 始め -> 終わり)
constexpr float kLaunchStreakLengthStart = 9.0f;
constexpr float kLaunchStreakLengthEnd = 15.0f;
constexpr float kLaunchStreakThickness = 0.30f;
constexpr float kLaunchStreakThicknessEnd = 0.06f;
constexpr float kLaunchStreakLife = 0.20f;
inline const Vector4 kLaunchStreakColor{ 0.85f, 0.70f, 1.00f, 1.0f };
// 衝撃波の輪
constexpr float kLaunchRingSizeStart = 1.0f;
constexpr float kLaunchRingSizeEnd = 5.0f;
constexpr float kLaunchRingLife = 0.26f;
inline const Vector4 kLaunchRingColor{ 0.75f, 0.50f, 1.00f, 1.0f };
// 後ろへ飛ぶ火花: 数、速さ、横へ散る速さ、寿命、長さと太さ(板の大きさ)
constexpr int kLaunchSparkCount = 14;
constexpr float kLaunchSparkSpeedMin = 8.0f;
constexpr float kLaunchSparkSpeedMax = 18.0f;
constexpr float kLaunchSparkSideSpeedMax = 3.0f;
constexpr float kLaunchSparkLifeMin = 0.20f;
constexpr float kLaunchSparkLifeMax = 0.35f;
constexpr float kLaunchSparkLengthMin = 1.8f;
constexpr float kLaunchSparkLengthMax = 3.4f;
constexpr float kLaunchSparkThicknessMin = 0.14f;
constexpr float kLaunchSparkThicknessMax = 0.24f;

// ===== 飛行中の火花(後ろへ飛ぶ細長い線) =====
constexpr float kFlightSparkSpeedMin = 6.0f;
constexpr float kFlightSparkSpeedMax = 14.0f;
constexpr float kFlightSparkSideSpeedMax = 2.5f;
constexpr float kFlightSparkLifeMin = 0.18f;
constexpr float kFlightSparkLifeMax = 0.32f;
constexpr float kFlightSparkLengthMin = 1.4f;
constexpr float kFlightSparkLengthMax = 2.6f;
constexpr float kFlightSparkThicknessMin = 0.12f;
constexpr float kFlightSparkThicknessMax = 0.20f;
// 線の、消えるときの長さと太さの割合
constexpr float kSparkEndLengthRatio = 0.30f;
constexpr float kSparkEndThicknessRatio = 0.60f;
inline const Vector4 kSparkColorHot{ 0.95f, 0.88f, 1.00f, 1.0f };
inline const Vector4 kSparkColorViolet{ 0.60f, 0.25f, 1.00f, 1.0f };

// ===== 着弾 =====
constexpr float kImpactFlashSizeStart = 2.0f;
constexpr float kImpactFlashSizeEnd = 6.5f;
constexpr float kImpactFlashLife = 0.20f;
inline const Vector4 kImpactFlashColor{ 0.92f, 0.80f, 1.00f, 0.95f };
constexpr float kImpactWideSizeStart = 3.0f;
constexpr float kImpactWideSizeEnd = 9.0f;
constexpr float kImpactWideLife = 0.32f;
inline const Vector4 kImpactWideColor{ 0.45f, 0.14f, 0.95f, 0.70f };
constexpr float kImpactStarSizeStart = 5.0f;
constexpr float kImpactStarSizeEnd = 9.0f;
constexpr float kImpactStarLife = 0.26f;
inline const Vector4 kImpactStarColor{ 0.95f, 0.85f, 1.00f, 1.0f };
constexpr int kImpactSparkCount = 28;
constexpr float kImpactSparkSpeedMin = 8.0f;
constexpr float kImpactSparkSpeedMax = 22.0f;
constexpr float kImpactSparkLifeMin = 0.25f;
constexpr float kImpactSparkLifeMax = 0.55f;
constexpr float kImpactRingSizeStart = 1.5f;
constexpr float kImpactRingSizeEnd = 7.0f;
constexpr float kImpactRingLife = 0.35f;
inline const Vector4 kImpactRingColor{ 0.70f, 0.40f, 1.00f, 1.0f };

// ===== 撃墜(着弾より、白く鋭く、火花が多い) =====
constexpr float kShotFlashSizeStart = 2.5f;
constexpr float kShotFlashSizeEnd = 8.0f;
constexpr float kShotFlashLife = 0.22f;
inline const Vector4 kShotFlashColor{ 1.00f, 0.97f, 1.00f, 1.0f };
constexpr float kShotWideSizeStart = 4.0f;
constexpr float kShotWideSizeEnd = 11.0f;
constexpr float kShotWideLife = 0.35f;
inline const Vector4 kShotWideColor{ 0.50f, 0.18f, 1.00f, 0.75f };
constexpr float kShotStarSizeStart = 6.0f;
constexpr float kShotStarSizeEnd = 11.0f;
constexpr float kShotStarLife = 0.30f;
inline const Vector4 kShotStarColor{ 1.00f, 0.95f, 1.00f, 1.0f };
constexpr int kShotSparkCount = 36;
constexpr float kShotSparkSpeedMin = 10.0f;
constexpr float kShotSparkSpeedMax = 26.0f;
constexpr float kShotSparkLifeMin = 0.30f;
constexpr float kShotSparkLifeMax = 0.65f;
// 輪は 2 重(大きな輪と、白い小さな輪)
constexpr float kShotRingSizeStart = 1.5f;
constexpr float kShotRingSizeEnd = 8.5f;
constexpr float kShotRingLife = 0.38f;
inline const Vector4 kShotRingColor{ 0.75f, 0.45f, 1.00f, 1.0f };
constexpr float kShotRingInnerSizeStart = 1.0f;
constexpr float kShotRingInnerSizeEnd = 5.0f;
constexpr float kShotRingInnerLife = 0.26f;
inline const Vector4 kShotRingInnerColor{ 1.00f, 0.95f, 1.00f, 1.0f };

// ===== 着弾と撃墜に共通 =====
// 星形の火花(星形の画像は、光条が細いので、板を大きめにする。大きさ: 始め -> 終わり)
constexpr float kSparkSizeStart = 1.0f;
constexpr float kSparkSizeEnd = 0.12f;
inline const Vector4 kSparkColor{ 0.80f, 0.55f, 1.00f, 1.0f };
// 火の粉(小さな光が、ゆっくり漂って消える)
constexpr int kEmberCount = 12;
constexpr float kEmberSpeedMin = 1.0f;
constexpr float kEmberSpeedMax = 3.5f;
constexpr float kEmberLifeMin = 0.5f;
constexpr float kEmberLifeMax = 0.9f;
constexpr float kEmberSizeStart = 0.22f;
constexpr float kEmberSizeEnd = 0.02f;
inline const Vector4 kEmberColor{ 0.65f, 0.35f, 1.00f, 0.9f };

// 画面に平行な面の、角度 angle の向き(長さ 1)
Vector3 PlaneDirection(const Vector3& right, const Vector3& up, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return { right.x * c + up.x * s, right.y * c + up.y * s, right.z * c + up.z * s };
}

// 向き(ワールド)を、画面に平行な面へ落としたときの角度(カメラの右を 0、上を +90 度)
float ScreenAngle(const Vector3& v, const Vector3& right, const Vector3& up) {
    return std::atan2(Dot(v, up), Dot(v, right));
}

float Random(float lo, float hi) {
    static std::mt19937 rng(20261126u); // 演出ごとに、少しずつ違う火花にするための、共有の乱数
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

float Mix(float a, float b, float t) { return a + (b - a) * t; }

// 柔らかい光の粒を 1 つ出す(その場で、大きさだけが変わる)
void EmitGlow(const Vector3& position, float sizeStart, float sizeEnd, float life, const Vector4& color) {
    const Vector3 endScale = { sizeEnd, sizeEnd, 1.0f };
    ParticleManager::GetInstance()->Emit(kGlowGroupName, position, { 0.0f, 0.0f, 0.0f }, { sizeStart, sizeStart, 1.0f },
                                         { 0.0f, 0.0f, 0.0f }, life, color, &endScale);
}

// 星形の閃光を 1 つ出す(その場で、大きさだけが変わる。板の向きはばらす)
void EmitStar(const Vector3& position, float sizeStart, float sizeEnd, float life, const Vector4& color) {
    const Vector3 endScale = { sizeEnd, sizeEnd, 1.0f };
    ParticleManager::GetInstance()->Emit(kFlareGroupName, position, { 0.0f, 0.0f, 0.0f }, { sizeStart, sizeStart, 1.0f },
                                         { 0.0f, 0.0f, Random(0.0f, kTwoPi) }, life, color, &endScale);
}

// 衝撃波の輪を 1 つ出す(広がって消える)
void EmitRing(const Vector3& position, float sizeStart, float sizeEnd, float life, const Vector4& color) {
    const Vector3 endScale = { sizeEnd, sizeEnd, 1.0f };
    ParticleManager::GetInstance()->Emit(kRingGroupName, position, { 0.0f, 0.0f, 0.0f }, { sizeStart, sizeStart, 1.0f },
                                         { 0.0f, 0.0f, 0.0f }, life, color, &endScale);
}

// 画面に平行な面で、四方へ飛ぶ星形の火花を count 個出す(角度を等分して、少しだけずらす)
void EmitRadialSparks(const Vector3& position, const Vector3& right, const Vector3& up, int count,
                      float speedMin, float speedMax, float lifeMin, float lifeMax) {
    auto* particles = ParticleManager::GetInstance();
    for (int i = 0; i < count; ++i) {
        const float angle = (static_cast<float>(i) + Random(-0.4f, 0.4f)) / static_cast<float>(count) * kTwoPi;
        const Vector3 direction = PlaneDirection(right, up, angle);
        const float speed = Random(speedMin, speedMax);
        const Vector3 velocity = { direction.x * speed, direction.y * speed, direction.z * speed };
        const Vector3 endScale = { kSparkSizeEnd, kSparkSizeEnd, 1.0f };
        // 星形の画像は、向きがそろうと不自然なので、1 つずつ、板の向き(Z 回り)をばらす
        particles->Emit(kFlareGroupName, position, velocity, { kSparkSizeStart, kSparkSizeStart, 1.0f },
                        { 0.0f, 0.0f, Random(0.0f, kTwoPi) }, Random(lifeMin, lifeMax), kSparkColor, &endScale);
    }
}

// 火の粉を出す(小さな光が、四方へゆっくり漂って消える)
void EmitEmbers(const Vector3& position, const Vector3& right, const Vector3& up) {
    auto* particles = ParticleManager::GetInstance();
    for (int i = 0; i < kEmberCount; ++i) {
        const Vector3 direction = PlaneDirection(right, up, Random(0.0f, kTwoPi));
        const float speed = Random(kEmberSpeedMin, kEmberSpeedMax);
        const Vector3 velocity = { direction.x * speed, direction.y * speed, direction.z * speed };
        const Vector3 endScale = { kEmberSizeEnd, kEmberSizeEnd, 1.0f };
        particles->Emit(kGlowGroupName, position, velocity, { kEmberSizeStart, kEmberSizeStart, 1.0f }, { 0.0f, 0.0f, 0.0f },
                        Random(kEmberLifeMin, kEmberLifeMax), kEmberColor, &endScale);
    }
}

// 玉の後ろへ飛ぶ、細長い火花を 1 つ出す(飛ぶ向きに伸びて見える。板を細長くして、画面の上での飛ぶ向きに合わせる)
void EmitBackwardSpark(const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up,
                       float speedMin, float speedMax, float sideSpeedMax, float lifeMin, float lifeMax,
                       float lengthMin, float lengthMax, float thicknessMin, float thicknessMax) {
    const Vector3 side = PlaneDirection(right, up, Random(0.0f, kTwoPi));
    const float sideSpeed = Random(0.0f, sideSpeedMax);
    const float backSpeed = Random(speedMin, speedMax);
    const Vector3 velocity = {
        -direction.x * backSpeed + side.x * sideSpeed,
        -direction.y * backSpeed + side.y * sideSpeed,
        -direction.z * backSpeed + side.z * sideSpeed };
    const float length = Random(lengthMin, lengthMax);
    const float thickness = Random(thicknessMin, thicknessMax);
    const Vector3 endScale = { length * kSparkEndLengthRatio, thickness * kSparkEndThicknessRatio, 1.0f };
    const float angle = ScreenAngle(velocity, right, up); // 板の向きは、その火花自身の飛ぶ向き(画面の上での)に合わせる
    const float hot = Random(0.0f, 1.0f);
    const Vector4 color = { Mix(kSparkColorViolet.x, kSparkColorHot.x, hot), Mix(kSparkColorViolet.y, kSparkColorHot.y, hot),
                            Mix(kSparkColorViolet.z, kSparkColorHot.z, hot), 1.0f };
    ParticleManager::GetInstance()->Emit(kStreakGroupName, position, velocity, { length, thickness, 1.0f },
                                         { 0.0f, 0.0f, angle }, Random(lifeMin, lifeMax), color, &endScale);
}
} // namespace

void BossOrbEffects::EmitLaunch(const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up) {
    BossEffectParticles::EnsureGroups(); // 粒のグループは、無ければ作る
    auto* particles = ParticleManager::GetInstance();

    EmitGlow(position, kLaunchFlashSizeStart, kLaunchFlashSizeEnd, kLaunchFlashLife, kLaunchFlashColor);
    EmitStar(position, kLaunchStarSizeStart, kLaunchStarSizeEnd, kLaunchStarLife, kLaunchStarColor);
    EmitRing(position, kLaunchRingSizeStart, kLaunchRingSizeEnd, kLaunchRingLife, kLaunchRingColor);

    // 進む向きへ伸びる光の筋(画面の上での、進む向きに合わせた細長い板)
    {
        const Vector3 endScale = { kLaunchStreakLengthEnd, kLaunchStreakThicknessEnd, 1.0f };
        particles->Emit(kStreakGroupName, position, { 0.0f, 0.0f, 0.0f }, { kLaunchStreakLengthStart, kLaunchStreakThickness, 1.0f },
                        { 0.0f, 0.0f, ScreenAngle(direction, right, up) }, kLaunchStreakLife, kLaunchStreakColor, &endScale);
    }

    // 後ろへ飛ぶ火花
    for (int i = 0; i < kLaunchSparkCount; ++i) {
        EmitBackwardSpark(position, direction, right, up, kLaunchSparkSpeedMin, kLaunchSparkSpeedMax, kLaunchSparkSideSpeedMax,
                          kLaunchSparkLifeMin, kLaunchSparkLifeMax, kLaunchSparkLengthMin, kLaunchSparkLengthMax,
                          kLaunchSparkThicknessMin, kLaunchSparkThicknessMax);
    }
}

void BossOrbEffects::EmitFlightSparks(const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up, int count) {
    BossEffectParticles::EnsureGroups();
    for (int i = 0; i < count; ++i) {
        EmitBackwardSpark(position, direction, right, up, kFlightSparkSpeedMin, kFlightSparkSpeedMax, kFlightSparkSideSpeedMax,
                          kFlightSparkLifeMin, kFlightSparkLifeMax, kFlightSparkLengthMin, kFlightSparkLengthMax,
                          kFlightSparkThicknessMin, kFlightSparkThicknessMax);
    }
}

void BossOrbEffects::EmitImpact(const Vector3& position, const Vector3& right, const Vector3& up) {
    BossEffectParticles::EnsureGroups();
    EmitGlow(position, kImpactFlashSizeStart, kImpactFlashSizeEnd, kImpactFlashLife, kImpactFlashColor);
    EmitGlow(position, kImpactWideSizeStart, kImpactWideSizeEnd, kImpactWideLife, kImpactWideColor);
    EmitStar(position, kImpactStarSizeStart, kImpactStarSizeEnd, kImpactStarLife, kImpactStarColor);
    EmitRing(position, kImpactRingSizeStart, kImpactRingSizeEnd, kImpactRingLife, kImpactRingColor);
    EmitRadialSparks(position, right, up, kImpactSparkCount, kImpactSparkSpeedMin, kImpactSparkSpeedMax, kImpactSparkLifeMin, kImpactSparkLifeMax);
    EmitEmbers(position, right, up);
}

void BossOrbEffects::EmitShotDown(const Vector3& position, const Vector3& right, const Vector3& up) {
    BossEffectParticles::EnsureGroups();
    EmitGlow(position, kShotFlashSizeStart, kShotFlashSizeEnd, kShotFlashLife, kShotFlashColor);
    EmitGlow(position, kShotWideSizeStart, kShotWideSizeEnd, kShotWideLife, kShotWideColor);
    EmitStar(position, kShotStarSizeStart, kShotStarSizeEnd, kShotStarLife, kShotStarColor);
    EmitRing(position, kShotRingSizeStart, kShotRingSizeEnd, kShotRingLife, kShotRingColor);
    EmitRing(position, kShotRingInnerSizeStart, kShotRingInnerSizeEnd, kShotRingInnerLife, kShotRingInnerColor);
    EmitRadialSparks(position, right, up, kShotSparkCount, kShotSparkSpeedMin, kShotSparkSpeedMax, kShotSparkLifeMin, kShotSparkLifeMax);
    EmitEmbers(position, right, up);
}
