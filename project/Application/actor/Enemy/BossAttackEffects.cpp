#include "BossAttackEffects.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/graphics/particle/ParticleManager.h"
#include "BossEffectParticles.h"
#include "Method.h"
#include <algorithm>
#include <cmath>

namespace {
using namespace BossAttackEffectDefaults;
using namespace BossEffectParticles;

constexpr float kTwoPi = 6.28318530718f;

// a から b へ、t(0 〜 1)で混ぜる
float Mix(float a, float b, float t) { return a + (b - a) * t; }

// 0 〜 1 の t を、始めと終わりをなめらかにして返す
float SmoothStep(float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return c * c * (3.0f - 2.0f * c);
}

// 0 〜 1 の t を、始めが速く、終わりが遅いように返す(広がって消える動き用)
float EaseOut(float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - c) * (1.0f - c);
}

// ビュー行列から、カメラの右と上(ワールド)を取り出す。ArcRing のビルボードと同じ作り方(回転だけを逆にして、行を読む)
void GetCameraAxes(const Matrix4x4& view, Vector3& right, Vector3& up) {
    Matrix4x4 billboard = view;
    billboard.m[3][0] = 0.0f;
    billboard.m[3][1] = 0.0f;
    billboard.m[3][2] = 0.0f;
    billboard = Inverse(billboard);
    right = { billboard.m[0][0], billboard.m[0][1], billboard.m[0][2] };
    up = { billboard.m[1][0], billboard.m[1][1], billboard.m[1][2] };
}

// 画面に平行な面の、角度 angle の向き(長さ 1)
Vector3 PlaneDirection(const Vector3& right, const Vector3& up, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return { right.x * c + up.x * s, right.y * c + up.y * s, right.z * c + up.z * s };
}
}

float BossAttackEffects::Random_(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

bool BossAttackEffects::EnsureInitialized_() {
    if (ringReady_) return true;
    if (initializeFailed_) return false;

    // 輪の板を作る。粒のグループは、アルファ合成の側(softAlpha)に置く(明るい空の上でも、色が白く飛ばない)
    ringReady_ = Renderer::GetInstance()->InitializeArcRing(ring_);
    if (ringReady_) {
        BossEffectParticles::EnsureGroups();
    }
    initializeFailed_ = !ringReady_;
    return ringReady_;
}

void BossAttackEffects::OnStageChanged(BossAttackStage stage, const BossAttackSpec& spec) {
    stage_ = stage;
    spec_ = spec;
    stageElapsed_ = 0.0f;
    emitCarry_ = 0.0f;
    switch (stage) {
    case BossAttackStage::Telegraph: stageSeconds_ = spec.telegraphSec; break;
    case BossAttackStage::Charge:    stageSeconds_ = spec.chargeSec; break;
    case BossAttackStage::Fire:      stageSeconds_ = kFireSeconds; break;
    case BossAttackStage::None:      stageSeconds_ = 0.0f; break;
    }
    if (stage == BossAttackStage::Fire) fireBurstPending_ = true;
    if (stage == BossAttackStage::None) visible_ = false;
}

void BossAttackEffects::Update(float deltaTime, const Vector3* muzzle) {
    // 口の動きの速さ(粒の目標を、動きの先へ置くため)。毎フレーム、位置の差から求める
    if (muzzle) {
        if (hasPreviousMuzzle_ && deltaTime > 0.0f) {
            muzzleVelocity_ = { (muzzle->x - previousMuzzle_.x) / deltaTime,
                                (muzzle->y - previousMuzzle_.y) / deltaTime,
                                (muzzle->z - previousMuzzle_.z) / deltaTime };
        }
        previousMuzzle_ = *muzzle;
        hasPreviousMuzzle_ = true;
    } else {
        hasPreviousMuzzle_ = false;
    }

    // 攻撃の最中でない、または出口の位置が分からないときは、何も出さない
    if (stage_ == BossAttackStage::None || !muzzle) {
        visible_ = false;
        return;
    }
    // 輪の板と粒のグループは、初めて使うときに作る(使わないボスでは、作らない)
    if (!EnsureInitialized_()) {
        visible_ = false;
        return;
    }

    stageElapsed_ += deltaTime;
    const float progress = (stageSeconds_ > 0.0f) ? std::clamp(stageElapsed_ / stageSeconds_, 0.0f, 1.0f) : 1.0f;
    position_ = *muzzle;
    GetCameraAxes(Renderer::GetInstance()->GetViewMatrix(), cameraRight_, cameraUp_);

    // 発射の瞬間の粒(口の位置で、1 回だけ)
    if (fireBurstPending_) {
        fireBurstPending_ = false;
        EmitFireBurst_();
    }
    // 溜めの間の、吸い込まれる粒
    if (stage_ == BossAttackStage::Charge) {
        EmitConverge_(deltaTime, progress);
    }

    // 発射の輪は、広がって消えたら終わり(隙の残りの間は、輪を出さない)
    if (stage_ == BossAttackStage::Fire && progress >= 1.0f) {
        visible_ = false;
        return;
    }

    // 回る速さ: 予告は一定、溜めで速くなり、発射では溜めの終わりの速さのまま
    float spinSpeed = kSpinTelegraph;
    if (stage_ == BossAttackStage::Charge) {
        spinSpeed = Mix(kSpinTelegraph, kSpinChargeEnd, SmoothStep(progress));
    } else if (stage_ == BossAttackStage::Fire) {
        spinSpeed = kSpinChargeEnd;
    }
    spin_ += spinSpeed * deltaTime;

    ApplyLook_(progress);
    visible_ = true;
}

void BossAttackEffects::ApplyLook_(float progress) {
    // 段ごとの、大きさ・切れ目・明るさ・濃さ(progress は、その段の中の進み具合 0 〜 1)
    float scale = kScaleTelegraphStart;
    float gap = kGapTelegraph;
    float intensity = kIntensityTelegraph;
    float alpha = 1.0f;
    switch (stage_) {
    case BossAttackStage::Telegraph:
        alpha = SmoothStep(progress / kTelegraphFadeInShare);
        scale = Mix(kScaleTelegraphStart, kScaleTelegraphEnd, SmoothStep(progress));
        break;
    case BossAttackStage::Charge: {
        // 輪は、溜めの終わりの少し前に口へ着き、残りは、口で強く光って発射を待つ
        const float arrive = SmoothStep(progress / kChargeArriveShare);
        scale = Mix(kScaleTelegraphEnd, kScaleChargeEnd, arrive);
        gap = Mix(kGapTelegraph, kGapChargeEnd, arrive);
        intensity = Mix(kIntensityTelegraph, kIntensityChargeEnd, SmoothStep(progress));
        break;
    }
    case BossAttackStage::Fire: {
        const float eased = EaseOut(progress);
        scale = Mix(kScaleChargeEnd, kScaleFireEnd, eased);
        gap = Mix(kGapChargeEnd, kGapFireEnd, eased);
        intensity = kIntensityChargeEnd;
        alpha = 1.0f - progress * progress; // 広がるにつれて薄れる
        break;
    }
    case BossAttackStage::None:
        break;
    }

    // 扇状弾は、弧の数を弾の数に合わせ、輪も大きくする
    uint32_t arcCount = kAimedArcCount;
    if (spec_.kind == BossAttackKind::Spread) {
        arcCount = static_cast<uint32_t>((std::max)(spec_.count, 1)); // max マクロとの衝突を避ける括弧
        scale *= kSpreadScaleMultiplier;
    }

    ArcRing::Params params;
    params.innerRadius = kRingInnerRadius;
    params.outerRadius = kRingOuterRadius;
    params.arcCount = arcCount;
    params.gapRatio = gap;
    params.rotation = spin_;
    params.edgeColor = kEdgeColor;
    params.coreColor = kCoreColor;
    params.intensity = intensity;
    params.alpha = alpha;
    ring_.SetParams(params);
    worldRadius_ = kQuadWorldRadius * scale;
}

void BossAttackEffects::EmitConverge_(float deltaTime, float progress) {
    // 出す数: 溜めが進むほど増える。端数は、次のフレームへ持ち越す
    emitCarry_ += Mix(kConvergeRateStart, kConvergeRateEnd, SmoothStep(progress)) * deltaTime;
    const int count = static_cast<int>(emitCarry_);
    emitCarry_ -= static_cast<float>(count);

    // 粒は、いまの輪の外側のあたり(画面に平行な面の上)から、口へ向かう
    const float ringRadius = worldRadius_ * kRingOuterRadius;
    auto* particles = ParticleManager::GetInstance();
    for (int i = 0; i < count; ++i) {
        const float life = Random_(kConvergeLifeMin, kConvergeLifeMax);
        const Vector3 direction = PlaneDirection(cameraRight_, cameraUp_, Random_(0.0f, kTwoPi));
        const float distance = ringRadius * Random_(kConvergeSpawnRadiusMin, kConvergeSpawnRadiusMax);
        const Vector3 start = { position_.x + direction.x * distance, position_.y + direction.y * distance, position_.z + direction.z * distance };
        // 寿命の終わりに、口(動きの先)へ着くように、速度を決める
        const Vector3 target = { position_.x + muzzleVelocity_.x * life, position_.y + muzzleVelocity_.y * life, position_.z + muzzleVelocity_.z * life };
        const Vector3 velocity = { (target.x - start.x) / life, (target.y - start.y) / life, (target.z - start.z) / life };

        const float jitter = 1.0f + Random_(-kConvergeSizeJitter, kConvergeSizeJitter);
        const float sizeStart = kConvergeSizeStart * jitter;
        const float sizeEnd = kConvergeSizeEnd * jitter;
        const Vector3 endScale = { sizeEnd, sizeEnd, 1.0f };
        particles->Emit(kGlowGroupName, start, velocity, { sizeStart, sizeStart, 1.0f }, { 0.0f, 0.0f, 0.0f }, life, kConvergeColor, &endScale);
    }
}

void BossAttackEffects::EmitFireBurst_() {
    auto* particles = ParticleManager::GetInstance();

    // 閃光: 口で、一瞬ふくらんで消える大きな光
    const Vector3 flashEnd = { kFlashSizeEnd, kFlashSizeEnd, 1.0f };
    particles->Emit(kGlowGroupName, position_, { 0.0f, 0.0f, 0.0f }, { kFlashSizeStart, kFlashSizeStart, 1.0f },
                    { 0.0f, 0.0f, 0.0f }, kFlashLife, kFlashColor, &flashEnd);

    // 火花: 輪の面に沿って、四方へ放射状に飛ぶ(角度を等分して、少しだけずらす)
    for (int i = 0; i < kFireSparkCount; ++i) {
        const float angle = (static_cast<float>(i) + Random_(-0.4f, 0.4f)) / static_cast<float>(kFireSparkCount) * kTwoPi;
        const Vector3 direction = PlaneDirection(cameraRight_, cameraUp_, angle);
        const float speed = Random_(kFireSparkSpeedMin, kFireSparkSpeedMax);
        const Vector3 velocity = { direction.x * speed + muzzleVelocity_.x, direction.y * speed + muzzleVelocity_.y, direction.z * speed + muzzleVelocity_.z };
        const Vector3 endScale = { kFireSparkSizeEnd, kFireSparkSizeEnd, 1.0f };
        // 星形の画像は、向きがそろうと不自然なので、1 つずつ、板の向き(Z 回り)をばらす
        particles->Emit(kFlareGroupName, position_, velocity, { kFireSparkSizeStart, kFireSparkSizeStart, 1.0f }, { 0.0f, 0.0f, Random_(0.0f, kTwoPi) },
                        Random_(kFireSparkLifeMin, kFireSparkLifeMax), kFireSparkColor, &endScale);
    }

    // 火の粉: 口のまわりに、ゆっくり漂って消える
    for (int i = 0; i < kEmberCount; ++i) {
        const Vector3 direction = PlaneDirection(cameraRight_, cameraUp_, Random_(0.0f, kTwoPi));
        const float speed = Random_(kEmberSpeedMin, kEmberSpeedMax);
        const Vector3 velocity = { direction.x * speed, direction.y * speed, direction.z * speed };
        const Vector3 endScale = { kEmberSizeEnd, kEmberSizeEnd, 1.0f };
        particles->Emit(kGlowGroupName, position_, velocity, { kEmberSizeStart, kEmberSizeStart, 1.0f }, { 0.0f, 0.0f, 0.0f },
                        Random_(kEmberLifeMin, kEmberLifeMax), kEmberColor, &endScale);
    }
}

void BossAttackEffects::Draw() {
    if (!visible_ || !ringReady_) return;
    auto* renderer = Renderer::GetInstance();
    // 描画の時点のカメラ(Renderer が持つ行列)で、板の向きを決める
    ring_.SetBillboardTransform(position_, worldRadius_, renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
    renderer->DrawArcRing(&ring_);
}

void BossAttackEffects::Stop() {
    stage_ = BossAttackStage::None;
    visible_ = false;
    fireBurstPending_ = false;
}
