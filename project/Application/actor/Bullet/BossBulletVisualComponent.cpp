#include "BossBulletVisualComponent.h"
#include "../Enemy/BossEffectParticles.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/graphics/particle/ParticleManager.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "Method.h"
#include <algorithm>
#include <cmath>

namespace {
using namespace BossBulletVisualDefaults;

constexpr float kTwoPi = 6.28318530718f;

// ビュー行列から、カメラの右と上と、視線の軸(ワールド)を取り出す。ArcRing のビルボードと同じ作り方(回転だけを逆にして、行を読む)。
// 視線の軸の向き(前か後ろか)は使わない(角度の絶対値だけを使う)
void GetCameraAxes(const Matrix4x4& view, Vector3& right, Vector3& up, Vector3& axis) {
    Matrix4x4 billboard = view;
    billboard.m[3][0] = 0.0f;
    billboard.m[3][1] = 0.0f;
    billboard.m[3][2] = 0.0f;
    billboard = Inverse(billboard);
    right = { billboard.m[0][0], billboard.m[0][1], billboard.m[0][2] };
    up = { billboard.m[1][0], billboard.m[1][1], billboard.m[1][2] };
    axis = { billboard.m[2][0], billboard.m[2][1], billboard.m[2][2] };
}

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

float Mix(float a, float b, float t) { return a + (b - a) * t; }
}

BossBulletVisualComponent::BossBulletVisualComponent(const Vector3& velocity)
    : velocity_(velocity), rng_(std::random_device{}()) { // 弾ごとに別の並びにする(同時に出た弾が、同じ火花を出さないように)
    speed_ = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    if (speed_ > 0.0001f) {
        direction_ = { velocity.x / speed_, velocity.y / speed_, velocity.z / speed_ };
    }

    RibbonTrail::Params trailParams;
    trailParams.minPointDistance = kTrailMinPointDistance;
    trailParams.lifetime = kTrailLifetime;
    trailParams.maxPoints = kTrailMaxPoints;
    trailParams.width = kTrailWidth;
    trailParams.tailWidthRatio = 0.0f; // 尾の先は、細く尖る
    trailParams.color = kTrailColor;
    trail_.Initialize(trailParams);
}

float BossBulletVisualComponent::Random_(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

void BossBulletVisualComponent::Update(float deltaTime) {
    if (!GetOwner()) return;
    age_ += deltaTime;

    const Vector3 position = GetOwner()->GetTransform().translate;
    trail_.Update(deltaTime, position);

    // 粒のグループは、初めて使うときに作る(無ければ)。粒は、画面に平行な面を基準に出すので、カメラの向きを取る
    if (!particlesReady_) {
        BossEffectParticles::EnsureGroups();
        particlesReady_ = true;
    }
    Vector3 right{ 1.0f, 0.0f, 0.0f };
    Vector3 up{ 0.0f, 1.0f, 0.0f };
    Vector3 axis{ 0.0f, 0.0f, 1.0f };
    GetCameraAxes(Renderer::GetInstance()->GetViewMatrix(), right, up, axis);
    EmitParticles_(deltaTime, position, right, up, axis);
}

void BossBulletVisualComponent::EmitParticles_(float deltaTime, const Vector3& position, const Vector3& cameraRight,
                                               const Vector3& cameraUp, const Vector3& cameraAxis) {
    auto* particles = ParticleManager::GetInstance();
    const float bulletAngle = ScreenAngle(direction_, cameraRight, cameraUp);

    // --- 光の尾の滲み: 弾の位置に、進む向きへ長い光を、毎フレーム置く(短い寿命で消える) ---
    {
        const Vector3 endScale = { kSmearLength * 0.6f, kSmearThickness * 0.6f, 1.0f };
        particles->Emit(BossEffectParticles::kGlowGroupName, position, { 0.0f, 0.0f, 0.0f }, { kSmearLength, kSmearThickness, 1.0f },
                        { 0.0f, 0.0f, bulletAngle }, kSmearLife, kSmearColor, &endScale);
    }

    // --- 細長い火花: 弾の後ろへ飛び、飛ぶ向きに伸びて見える ---
    streakCarry_ += kStreakRate * deltaTime;
    const int streakCount = static_cast<int>(streakCarry_);
    streakCarry_ -= static_cast<float>(streakCount);
    for (int i = 0; i < streakCount; ++i) {
        const float life = Random_(kStreakLifeMin, kStreakLifeMax);
        const Vector3 side = PlaneDirection(cameraRight, cameraUp, Random_(0.0f, kTwoPi));
        const float sideSpeed = Random_(0.0f, kStreakSideSpeedMax);
        const float backSpeed = Random_(kStreakBackSpeedMin, kStreakBackSpeedMax);
        const Vector3 velocity = {
            -direction_.x * backSpeed + side.x * sideSpeed,
            -direction_.y * backSpeed + side.y * sideSpeed,
            -direction_.z * backSpeed + side.z * sideSpeed };
        const Vector3 start = {
            position.x + side.x * sideSpeed * kStreakSpawnSpread,
            position.y + side.y * sideSpeed * kStreakSpawnSpread,
            position.z + side.z * sideSpeed * kStreakSpawnSpread };
        const float length = Random_(kStreakLengthMin, kStreakLengthMax);
        const float thickness = Random_(kStreakThicknessMin, kStreakThicknessMax);
        const Vector3 scale = { length, thickness, 1.0f };
        const Vector3 endScale = { length * kStreakEndLengthRatio, thickness * kStreakEndThicknessRatio, 1.0f };
        // 板の向きは、その火花自身の飛ぶ向き(画面の上での)に合わせる
        const float streakAngle = ScreenAngle(velocity, cameraRight, cameraUp);
        const float hot = Random_(0.0f, 1.0f);
        const Vector4 color = { Mix(kStreakColorRed.x, kStreakColorHot.x, hot), Mix(kStreakColorRed.y, kStreakColorHot.y, hot),
                                Mix(kStreakColorRed.z, kStreakColorHot.z, hot), 1.0f };
        particles->Emit(BossEffectParticles::kStreakGroupName, start, velocity, scale, { 0.0f, 0.0f, streakAngle }, life, color, &endScale);
    }

    // --- 衝撃波の輪: 一定の間隔で、その場に残す ---
    // 輪の面は、弾の進む向きに垂直。画面では、「進む向きに平たい楕円」に見える:
    //   長い軸(輪の直径)は、画面での進む向きに垂直。短い軸は、直径 x |進む向きと視線の軸の内積|(視線に沿って飛ぶなら、真円)。
    // 粒の板は、画面に正対しているので、板の x 軸を進む向きに合わせ(roll)、x 方向を短く、y 方向を長くして、楕円にする
    const float minorRatio = (std::max)(std::abs(Dot(direction_, cameraAxis)), kRingMinorRatioMin);
    ringTimer_ += deltaTime;
    while (ringTimer_ >= kRingInterval) {
        ringTimer_ -= kRingInterval;
        const int kind = ringCount_ % kRingKinds;
        ++ringCount_;
        const Vector4& color = (kind == 0) ? kRingColorWhite : (kind == 1) ? kRingColorRed : kRingColorDark;
        const Vector3 velocity = { -direction_.x * kRingBackSpeed, -direction_.y * kRingBackSpeed, -direction_.z * kRingBackSpeed };
        const Vector3 startScale = { kRingSizeStart * minorRatio, kRingSizeStart, 1.0f };
        const Vector3 endScale = { kRingSizeEnd * minorRatio, kRingSizeEnd, 1.0f };
        particles->Emit(BossEffectParticles::kRingGroupName, position, velocity, startScale, { 0.0f, 0.0f, bulletAngle }, kRingLife, color, &endScale);
    }
}

void BossBulletVisualComponent::DrawAfterClouds() {
    if (!GetOwner() || initializeFailed_) return;

    auto* renderer = Renderer::GetInstance();
    const Vector3 position = GetOwner()->GetTransform().translate;

    // 帯の尾(アルファ合成)。芯より先に描く(芯が、尾の先端の上に重なる)
    renderer->DrawRibbon(trail_, BlendMode::Alpha);

    // 芯は、初めて描くときに作る(失敗しても、見た目が出ないだけで、弾の動きと当たり判定は続く)
    if (!coreReady_) {
        coreReady_ = renderer->InitializeArcRing(core_);
        initializeFailed_ = !coreReady_;
        if (!coreReady_) return;
    }

    // 光の玉: 中心を原点にした帯(内側の半径を負にする)で、中心が最も濃い円盤にする。外へ、赤い光がにじむ
    ArcRing::Params params;
    params.innerRadius = -kCoreRadius;
    params.outerRadius = kCoreRadius;
    params.arcCount = 1;
    params.gapRatio = 0.0f; // 切れ目なし
    params.edgeSoftness = 0.5f;
    params.haloStrength = kCoreHaloStrength;
    params.haloWidth = kCoreHaloWidth;
    params.edgeColor = kCoreEdgeColor;
    params.coreColor = kCoreCoreColor;
    params.corePower = kCoreCorePower;
    params.intensity = kCoreIntensity + kCorePulseAmount * std::sin(age_ * kCorePulseSpeed);
    core_.SetParams(params);
    core_.SetBillboardTransform(position, kCoreQuadWorldRadius, renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
    renderer->DrawArcRing(&core_);
}
