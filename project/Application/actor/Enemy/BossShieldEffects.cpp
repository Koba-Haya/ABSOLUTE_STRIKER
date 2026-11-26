#include "BossShieldEffects.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/graphics/particle/ParticleManager.h"
#include "BossEffectParticles.h"
#include "Method.h"
#include <algorithm>
#include <cmath>

namespace {
using namespace BossShieldEffectDefaults;

constexpr float kTwoPi = 6.28318530718f;
// 板の中の、輪(円盤)の内側と外側の半径。画面全体を覆う平らな色にするため、板より十分大きくして、ぼかしを 0 にする
constexpr float kFlashInnerRadius = -2.0f;
constexpr float kFlashOuterRadius = 2.0f;

float Mix(float a, float b, float t) { return a + (b - a) * t; }

// 0 〜 1 の t を、始めが速く、終わりが遅いように返す
float EaseOut(float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - c) * (1.0f - c);
}

Vector3 Normalized(const Vector3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    constexpr float kMinLength = 0.0001f;
    if (len < kMinLength) return { 0.0f, 0.0f, -1.0f };
    return { v.x / len, v.y / len, v.z / len };
}

Vector3 CameraPositionOf(const Matrix4x4& view) {
    const Matrix4x4 inverse = Inverse(view);
    return { inverse.m[3][0], inverse.m[3][1], inverse.m[3][2] };
}
}

bool BossShieldEffects::EnsureInitialized_() {
    if (ready_) return true;
    if (initializeFailed_) return false;
    auto* renderer = Renderer::GetInstance();
    ready_ = renderer->InitializeShieldDome(dome_);
    flashReady_ = renderer->InitializeArcRing(flash_);
    initializeFailed_ = !ready_;
    if (ready_) BossEffectParticles::EnsureGroups();
    return ready_;
}

float BossShieldEffects::Random_(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

Vector3 BossShieldEffects::RandomDirection_() {
    const float z = Random_(-1.0f, 1.0f);
    const float phi = Random_(0.0f, kTwoPi);
    const float r = std::sqrt((std::max)(0.0f, 1.0f - z * z)); // max マクロとの衝突を避ける括弧
    return { r * std::cos(phi), r * std::sin(phi), z };
}

void BossShieldEffects::Update(float deltaTime, const BossShieldView& view, const Vector3& center, const Vector3& cameraForward) {
    view_ = view;
    center_ = center;
    cameraForward_ = cameraForward;
    time_ += deltaTime;
    flashAlpha_ = 0.0f;

    // 被弾の波紋: 古くなったものを消す
    for (auto& hit : hits_) hit.age += deltaTime;
    hits_.erase(std::remove_if(hits_.begin(), hits_.end(), [](const ShieldDome::Hit& hit) { return hit.age >= kRippleLife; }), hits_.end());

    if (view.stage == BossShieldStage::None) {
        visible_ = false;
        hits_.clear();
        previousStage_ = BossShieldStage::None;
        return;
    }
    // ドームは、初めて出すときに作る(シールドを使わないボスでは、作らない)。失敗しても、演出が出ないだけで、ゲームは続ける
    if (!EnsureInitialized_()) {
        visible_ = false;
        return;
    }

    // 段が変わった瞬間の演出(砕けた瞬間の破片の火花)
    if (view.stage == BossShieldStage::Break && previousStage_ != BossShieldStage::Break) {
        EmitBreakBurst_();
    }
    previousStage_ = view.stage;

    // 段ごとの、半径、現れ方、砕け方、濃さ、フラッシュ
    const float p = view.stageProgress;
    radius_ = kDomeRadius;
    appear_ = 1.0f;
    breakProgress_ = 0.0f;
    alpha_ = 1.0f;
    switch (view.stage) {
    case BossShieldStage::Deploy:
        appear_ = p;
        radius_ = kDomeRadius * Mix(kDeployStartScale, 1.0f, EaseOut(p));
        break;
    case BossShieldStage::Hold:
        radius_ = kDomeRadius * (1.0f + kHoldPulseAmount * std::sin(time_ * kHoldPulseSpeed));
        break;
    case BossShieldStage::Break: {
        // 砕ける進み具合は、最後の面が飛び終える(1 + 破片の長さ)まで進める
        const ShieldDome::Params defaults;
        breakProgress_ = p * (1.0f + defaults.shardSpan);
        radius_ = kDomeRadius * Mix(1.0f, kBreakExpandScale, EaseOut(p));
        // 成功のフラッシュ: 弱く、短い
        const float flashP = std::clamp(p / kBreakFlashShare, 0.0f, 1.0f);
        flashAlpha_ = kBreakFlashAlpha * (1.0f - flashP) * (1.0f - flashP);
        break;
    }
    case BossShieldStage::Burst:
        radius_ = kDomeRadius * Mix(1.0f, kBurstEndScale, EaseOut(p));
        alpha_ = 1.0f - p;
        // 失敗のフラッシュ: 最初に画面全体が真っ白になり、すぐ薄れる
        flashAlpha_ = std::pow(1.0f - p, kBurstFlashPower);
        break;
    case BossShieldStage::None:
        break;
    }
    visible_ = true;
}

void BossShieldEffects::EmitBreakBurst_() {
    auto* particles = ParticleManager::GetInstance();
    // 破片の火花: ドームの表面の、ランダムな点から、外へ放射状に飛ぶ
    for (int i = 0; i < kBreakSparkCount; ++i) {
        const Vector3 direction = RandomDirection_();
        const Vector3 start = { center_.x + direction.x * kDomeRadius, center_.y + direction.y * kDomeRadius, center_.z + direction.z * kDomeRadius };
        const float speed = Random_(kBreakSparkSpeedMin, kBreakSparkSpeedMax);
        const Vector3 velocity = { direction.x * speed, direction.y * speed, direction.z * speed };
        const float mix = Random_(0.0f, 1.0f);
        const Vector4 color = { Mix(kBreakSparkColorViolet.x, kBreakSparkColorWhite.x, mix), Mix(kBreakSparkColorViolet.y, kBreakSparkColorWhite.y, mix),
                                Mix(kBreakSparkColorViolet.z, kBreakSparkColorWhite.z, mix), 1.0f };
        const Vector3 endScale = { kBreakSparkEndSize, kBreakSparkEndSize, 1.0f };
        particles->Emit(BossEffectParticles::kFlareGroupName, start, velocity, { kBreakSparkSize, kBreakSparkSize, 1.0f },
                        { 0.0f, 0.0f, Random_(0.0f, kTwoPi) }, Random_(kBreakSparkLifeMin, kBreakSparkLifeMax), color, &endScale);
    }
    // 中心の光の玉: 砕けた瞬間に、ふくらんで消える
    const Vector3 flashEnd = { kBreakFlashEndSize, kBreakFlashEndSize, 1.0f };
    particles->Emit(BossEffectParticles::kGlowGroupName, center_, { 0.0f, 0.0f, 0.0f }, { kBreakFlashSize, kBreakFlashSize, 1.0f },
                    { 0.0f, 0.0f, 0.0f }, kBreakFlashLife, kBreakFlashColor, &flashEnd);
}

void BossShieldEffects::AddHit(const Vector3& worldPosition) {
    // ドームがあるとき(展開と維持)だけ、波紋を出す
    if (view_.stage != BossShieldStage::Deploy && view_.stage != BossShieldStage::Hold) return;
    if (!ready_) return;

    // 被弾の点から、ドームの表面の交点を求める。カメラから被弾の点へ向かう線が、ドームの手前の面を貫く点
    // (弾は、カメラ側から飛んでくるので、貫く点が、見える側の表面になる)
    const Vector3 camera = CameraPositionOf(Renderer::GetInstance()->GetViewMatrix());
    const Vector3 rayDirection = Normalized({ worldPosition.x - camera.x, worldPosition.y - camera.y, worldPosition.z - camera.z });
    const Vector3 toCenter = { camera.x - center_.x, camera.y - center_.y, camera.z - center_.z };
    const float b = toCenter.x * rayDirection.x + toCenter.y * rayDirection.y + toCenter.z * rayDirection.z;
    const float c = toCenter.x * toCenter.x + toCenter.y * toCenter.y + toCenter.z * toCenter.z - radius_ * radius_;
    const float discriminant = b * b - c;

    Vector3 entry = worldPosition; // 交点が求まらないときは、被弾の点そのもの
    if (discriminant >= 0.0f) {
        const float t = -b - std::sqrt(discriminant); // 手前の交点
        if (t > 0.0f) {
            entry = { camera.x + rayDirection.x * t, camera.y + rayDirection.y * t, camera.z + rayDirection.z * t };
        }
    }
    const Vector3 normal = Normalized({ entry.x - center_.x, entry.y - center_.y, entry.z - center_.z });

    // 波紋: 交点の向きから、表面を広がる。同時に出せる数を超えたら、いちばん古い波紋を捨てる
    if (hits_.size() >= ShieldDome::kMaxHits) hits_.erase(hits_.begin());
    ShieldDome::Hit hit;
    hit.direction = normal;
    hit.age = 0.0f;
    hits_.push_back(hit);

    // 衝撃: 交点に、光の玉と、四方へ飛ぶ火花
    auto* particles = ParticleManager::GetInstance();
    const Vector3 flashEnd = { kImpactFlashEndSize, kImpactFlashEndSize, 1.0f };
    particles->Emit(BossEffectParticles::kGlowGroupName, entry, { 0.0f, 0.0f, 0.0f }, { kImpactFlashSize, kImpactFlashSize, 1.0f },
                    { 0.0f, 0.0f, 0.0f }, kImpactFlashLife, kImpactColor, &flashEnd);
    for (int i = 0; i < kImpactSparkCount; ++i) {
        // 表面に沿って散る向き: 外向きの法線に垂直なランダムな向き
        Vector3 tangent = RandomDirection_();
        const float along = tangent.x * normal.x + tangent.y * normal.y + tangent.z * normal.z;
        tangent = Normalized({ tangent.x - normal.x * along, tangent.y - normal.y * along, tangent.z - normal.z * along });
        const float sideSpeed = Random_(0.0f, kImpactSparkSideSpeedMax);
        const float normalSpeed = Random_(kImpactSparkNormalSpeedMin, kImpactSparkNormalSpeedMax);
        const Vector3 velocity = { normal.x * normalSpeed + tangent.x * sideSpeed, normal.y * normalSpeed + tangent.y * sideSpeed,
                                   normal.z * normalSpeed + tangent.z * sideSpeed };
        const Vector3 endScale = { kImpactSparkEndSize, kImpactSparkEndSize, 1.0f };
        particles->Emit(BossEffectParticles::kFlareGroupName, entry, velocity, { kImpactSparkSize, kImpactSparkSize, 1.0f },
                        { 0.0f, 0.0f, Random_(0.0f, kTwoPi) }, Random_(kImpactSparkLifeMin, kImpactSparkLifeMax), kImpactColor, &endScale);
    }
}

void BossShieldEffects::Draw() {
    if (!visible_ || !ready_) return;
    auto* renderer = Renderer::GetInstance();

    ShieldDome::Params params;
    params.cameraPosition = CameraPositionOf(renderer->GetViewMatrix());
    params.time = time_;
    params.hpRatio = view_.hpRatio;
    params.breakProgress = breakProgress_;
    params.appear = appear_;
    params.warning = view_.warning ? 1.0f : 0.0f;
    params.alpha = alpha_;
    params.rippleLife = kRippleLife;
    params.hitCount = static_cast<uint32_t>(hits_.size());
    for (size_t i = 0; i < hits_.size() && i < ShieldDome::kMaxHits; ++i) {
        params.hits[i] = hits_[i];
    }
    dome_.SetParams(params);
    dome_.SetTransform(center_, radius_, renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
    renderer->DrawShieldDome(&dome_);
}

void BossShieldEffects::DrawFlash() {
    if (!flashReady_ || flashAlpha_ <= 0.0f) return;
    auto* renderer = Renderer::GetInstance();

    // 画面全体を覆う白い板: カメラの前に、ビルボードの輪(ぼかし 0 の円盤)を、画面より十分大きく置く
    ArcRing::Params params;
    params.innerRadius = kFlashInnerRadius;
    params.outerRadius = kFlashOuterRadius;
    params.arcCount = 1;
    params.gapRatio = 0.0f;
    params.edgeSoftness = 0.0f;
    params.haloStrength = 0.0f;
    params.edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    params.coreColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    params.alpha = flashAlpha_;
    flash_.SetParams(params);

    const Vector3 eye = CameraPositionOf(renderer->GetViewMatrix());
    const Vector3 position = { eye.x + cameraForward_.x * kFlashDistance, eye.y + cameraForward_.y * kFlashDistance,
                               eye.z + cameraForward_.z * kFlashDistance };
    flash_.SetBillboardTransform(position, kFlashHalfSize, renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
    renderer->DrawArcRing(&flash_);
}

void BossShieldEffects::Stop() {
    view_ = BossShieldView{};
    previousStage_ = BossShieldStage::None;
    visible_ = false;
    flashAlpha_ = 0.0f;
    hits_.clear();
}
