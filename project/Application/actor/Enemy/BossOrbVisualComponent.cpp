#include "BossOrbVisualComponent.h"
#include "BossOrbComponent.h"
#include "BossOrbEffects.h"
#include "BossOrbTags.h"
#include "EnergyRibbonFadeComponent.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "Method.h"
#include <memory>
#include <algorithm>
#include <cmath>

namespace {
using namespace BossOrbVisualDefaults;

// この値より長さが短いベクトルは、向きを持たないものとして扱う
constexpr float kMinAxisLength = 0.0001f;
// 軌道の面の向きと、基準の向きが、平行に近いと見なす内積の値(外積の向きが決まらなくなるので、別の基準へ替える)
constexpr float kParallelDot = 0.9f;
// RibbonTrail の寿命(秒)。SetPath で、年齢の割合から age を作るときの基準(割合 x この値)。値そのものに意味はない
constexpr float kRibbonLifetime = 1.0f;
// 切り離した尾の GameObject の名前
constexpr const char* kTrailFadeObjectName = "BossOrbTrailFade";

// 0 〜 1 の t を、始めが速く、終わりが遅いように返す
float EaseOut(float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - c) * (1.0f - c);
}

float Mix(float a, float b, float t) { return a + (b - a) * t; }

// ビュー行列から、カメラの右と上と、視線の軸と、カメラの位置(ワールド)を取り出す。
// 回転だけを逆にして、行を読む(ArcRing のビルボードと同じ作り方)。視線の軸の向き(前か後ろか)は、使わない側で決める
void GetCameraFrame(const Matrix4x4& view, Vector3& right, Vector3& up, Vector3& axis, Vector3& position) {
    const Matrix4x4 inverse = Inverse(view);
    position = { inverse.m[3][0], inverse.m[3][1], inverse.m[3][2] };
    Matrix4x4 billboard = view;
    billboard.m[3][0] = 0.0f;
    billboard.m[3][1] = 0.0f;
    billboard.m[3][2] = 0.0f;
    billboard = Inverse(billboard);
    right = { billboard.m[0][0], billboard.m[0][1], billboard.m[0][2] };
    up = { billboard.m[1][0], billboard.m[1][1], billboard.m[1][2] };
    axis = { billboard.m[2][0], billboard.m[2][1], billboard.m[2][2] };
}

// 3 つの軸 x, y, z を、それぞれ cx, cy, cz 倍して足した値
Vector3 Combine(const Vector3& x, float cx, const Vector3& y, float cy, const Vector3& z, float cz) {
    return { x.x * cx + y.x * cy + z.x * cz, x.y * cx + y.y * cy + z.y * cz, x.z * cx + y.z * cy + z.z * cz };
}

float DistanceBetween(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
} // namespace

BossOrbVisualComponent::BossOrbVisualComponent(AbsoluteEngine::ISpawner& spawner) : spawner_(spawner) {
    // 帯の部分の置き場: 1 本につき、最大の部分の数だけ。形と幅は、毎フレーム決める(ここでは、先細りと寿命だけ)
    RibbonTrail::Params params;
    params.lifetime = kRibbonLifetime;
    params.width = kRibbonWidth;
    params.tailWidthRatio = kRibbonTailWidthRatio;
    runPool_.resize(kRibbonCount * kMaxRunsPerRibbon);
    for (auto& trail : runPool_) trail.Initialize(params);
    runs_.reserve(runPool_.size());

    // 飛行中の尾: 玉の見た目の位置を、距離ごとに記録する。先端が太く、尾の先は細く尖る
    RibbonTrail::Params trailParams;
    trailParams.minPointDistance = kTrailMinPointDistance;
    trailParams.lifetime = kTrailLifetime;
    trailParams.maxPoints = kTrailMaxPoints;
    trailParams.width = kTrailWidth;
    trailParams.tailWidthRatio = 0.0f;
    trail_.Initialize(trailParams);
}

EnergyRibbonStyle BossOrbVisualComponent::MakeTrailStyle_(float time) const {
    EnergyRibbonStyle style;
    style.glowColor = kOrbGlowColor;
    style.coreColor = kOrbCoreColor;
    style.outlineColor = kOrbOutlineColor;
    style.time = time;
    style.seed = seed_;
    style.intensity = kOrbIntensity * kTrailIntensity;
    style.flowSpeed = kTrailFlowSpeed;
    return style;
}

void BossOrbVisualComponent::Finish_(const BossOrbLook& look, const Vector3& position, float fade) {
    finished_ = true;

    // 終わり方に応じた演出(自機に重なった、島に遮られた -> 着弾。撃ち落とされた -> 撃墜。消された玉は、薄れて消えるだけ)
    if (look.end == BossOrbEnd::Impact || look.end == BossOrbEnd::ShotDown) {
        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        Vector3 axis{ 0.0f, 0.0f, 1.0f };
        Vector3 cameraPosition{ 0.0f, 0.0f, 0.0f };
        GetCameraFrame(Renderer::GetInstance()->GetViewMatrix(), right, up, axis, cameraPosition);
        if (look.end == BossOrbEnd::ShotDown) {
            BossOrbEffects::EmitShotDown(position, right, up);
        } else {
            BossOrbEffects::EmitImpact(position, right, up);
        }
    }

    // 尾を、玉から切り離して残す(玉と一緒に尾が一瞬で消えて、光の帯がぶつ切りに見えるのを防ぐ)。飛んでいない玉は、尾が無い
    if (trailStarted_) {
        trail_.Freeze();
        if (trail_.HasRibbon()) {
            trail_.SetColor({ 1.0f, 1.0f, 1.0f, fade });
            auto fadeObject = std::make_shared<AbsoluteEngine::GameObject>(kTrailFadeObjectName);
            fadeObject->SetTag(BossOrbTags::kTrail); // 死亡演出や撃破演出で、玉と一緒に消す対象の目印
            auto fadeComponent = std::make_unique<EnergyRibbonFadeComponent>();
            fadeComponent->Initialize(trail_, MakeTrailStyle_(look.age));
            fadeObject->AddComponent(std::move(fadeComponent));
            spawner_.Spawn(fadeObject);
        }
    }

    GetOwner()->Destroy();
}

void BossOrbVisualComponent::Update(float deltaTime) {
    if (!GetOwner() || finished_) return;
    const auto* orbComponent = GetOwner()->GetComponent<BossOrbComponent>();
    if (!orbComponent) return;

    const BossOrbLook& look = orbComponent->GetLook();
    const Vector3 position = {
        GetOwner()->GetTransform().translate.x + look.spiralOffset.x,
        GetOwner()->GetTransform().translate.y + look.spiralOffset.y,
        GetOwner()->GetTransform().translate.z + look.spiralOffset.z,
    };

    // 玉が終わった: 後始末をする(玉の動きの部品は、終わったことを伝えるだけで、GameObject は壊さない)
    if (look.end != BossOrbEnd::None) {
        Finish_(look, position, 1.0f - look.dismiss);
        return;
    }

    // 飛行中は、玉の見た目の位置を、尾に記録する
    if (look.flying) {
        trail_.Update(deltaTime, position);
        trailStarted_ = true;

        Vector3 right{ 1.0f, 0.0f, 0.0f };
        Vector3 up{ 0.0f, 1.0f, 0.0f };
        Vector3 axis{ 0.0f, 0.0f, 1.0f };
        Vector3 cameraPosition{ 0.0f, 0.0f, 0.0f };
        GetCameraFrame(Renderer::GetInstance()->GetViewMatrix(), right, up, axis, cameraPosition);

        // 飛び出した最初のフレームに、発射の演出(星形の閃光、光の筋、衝撃波の輪、後ろへ飛ぶ火花)を、1 回だけ出す
        if (!launchEffectDone_) {
            launchEffectDone_ = true;
            BossOrbEffects::EmitLaunch(position, look.direction, right, up);
        }

        // 飛行中は、後ろへ飛ぶ細長い火花を出し続ける(進む向きが、画面の上でも分かる)。端数は、次のフレームへ持ち越す
        sparkCarry_ += kFlightSparkRate * deltaTime;
        const int sparkCount = static_cast<int>(sparkCarry_);
        sparkCarry_ -= static_cast<float>(sparkCount);
        BossOrbEffects::EmitFlightSparks(position, look.direction, right, up, sparkCount);
    }
}

bool BossOrbVisualComponent::EnsureInitialized_() {
    if (ready_) return true;
    if (initializeFailed_) return false;

    auto* renderer = Renderer::GetInstance();
    ready_ = renderer->InitializeEnergyOrb(orb_) && renderer->InitializeArcRing(flash_);
    initializeFailed_ = !ready_;
    return ready_;
}

void BossOrbVisualComponent::BuildRibbons_(const Vector3& center, const Vector3& right, const Vector3& up, const Vector3& axis,
                                           const Vector3& cameraPosition, float orbit, float age, float charge) {
    runs_.clear();
    const float centerDistance = DistanceBetween(center, cameraPosition);

    for (std::size_t i = 0; i < kRibbonCount; ++i) {
        // --- 軌道の面: カメラから見た傾き(角度と方位)で、面の法線を決め、面の中の 2 本の軸(u, v)を作る ---
        const float tilt = kRibbonTiltAngles[i];
        const float azimuth = kRibbonTiltAzimuths[i];
        const Vector3 normal = Normalize(Combine(right, std::sin(tilt) * std::cos(azimuth),
                                                 up, std::sin(tilt) * std::sin(azimuth),
                                                 axis, std::cos(tilt)));
        const Vector3 reference = (std::abs(Dot(normal, up)) < kParallelDot) ? up : right;
        Vector3 u = Cross(normal, reference);
        if (Length(u) < kMinAxisLength) u = right;
        u = Normalize(u);
        const Vector3 v = Cross(normal, u);

        // --- 帯の位置: 先端(k = 0)から尾(k = 最後)へ、軌道を逆向きにたどる。先端の角度は、時間で進む(速さは、ゆれる) ---
        const float ratio = (kRibbonCount > 1) ? static_cast<float>(i) / static_cast<float>(kRibbonCount - 1) : 0.0f;
        const float direction = (i % 2 == 0) ? 1.0f : -1.0f; // 軌道ごとに、回る向きを変える
        const float speed = Mix(kRibbonSpeedMin, kRibbonSpeedMax, ratio) * (1.0f + kRibbonChargeSpeedBoost * charge);
        const float phase = static_cast<float>(i) * 1.7f;
        const float headAngle = direction * (speed * age + kRibbonSpeedWobble * std::sin(kRibbonSpeedWobbleRate * age + phase)) + phase;

        std::array<Vector3, kRibbonPointCount> positions{};
        std::array<bool, kRibbonPointCount> isBack{};
        for (std::size_t k = 0; k < kRibbonPointCount; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(kRibbonPointCount - 1); // 先端 0 から尾 1
            const float angle = headAngle - direction * t * kRibbonArcRadians;
            const float radius = kRibbonOrbitRadius * orbit * (1.0f + kRibbonOrbitWobble * std::sin(kRibbonOrbitWobbleWaves * angle + phase));
            // 軌道の面の中の位置に、面に垂直な向き(手前と奥)への波を足す。帯が、平らな輪ではなく、立体にうねる
            const float lift = radius * kRibbonOutOfPlaneWobble * std::sin(kRibbonOutOfPlaneWaves * angle + phase * 1.3f);
            const Vector3 offset = Combine(u, std::cos(angle) * radius, v, std::sin(angle) * radius, normal, lift);
            positions[k] = { center.x + offset.x, center.y + offset.y, center.z + offset.z };
            // カメラから見て、玉の中心より遠い点は、玉の奥(玉に隠れて見える側)
            isBack[k] = DistanceBetween(positions[k], cameraPosition) > centerDistance;
        }

        // --- 奥と手前で、一続きの部分に分ける。部分のつなぎ目の点は、両側の部分に含める(隙間が空かないように)。
        //     RibbonTrail の点は、尾(古い点)から先端(新しい点)の順に並べる ---
        std::vector<RibbonTrail::Point> current;
        bool currentBack = false;
        std::size_t runCount = 0; // この帯で、いままでに作った部分の数(置き場は、1 本につき kMaxRunsPerRibbon 個)
        auto flush = [&](std::vector<RibbonTrail::Point>& points, bool back) {
            if (points.size() < 2 || runCount >= kMaxRunsPerRibbon) return; // 点が足りない部分と、置き場を超える部分は、描かない
            const std::size_t poolIndex = i * kMaxRunsPerRibbon + runCount;
            runPool_[poolIndex].SetPath(points);
            runs_.push_back({ poolIndex, back, static_cast<float>(i) * kRibbonSeedStep });
            ++runCount;
        };
        for (std::size_t j = 0; j < kRibbonPointCount; ++j) {
            const std::size_t k = kRibbonPointCount - 1 - j; // 尾から先端へ
            const float t = static_cast<float>(k) / static_cast<float>(kRibbonPointCount - 1);
            const RibbonTrail::Point point{ positions[k], t * kRibbonLifetime };
            if (current.empty()) {
                currentBack = isBack[k];
            } else if (isBack[k] != currentBack) {
                current.push_back(point); // つなぎ目の点を、いまの部分の終わりにも含める
                flush(current, currentBack);
                current.clear();
                currentBack = isBack[k];
            }
            current.push_back(point);
        }
        flush(current, currentBack);
    }
}

void BossOrbVisualComponent::DrawRuns_(bool back, float alpha, float time, float charge) {
    auto* renderer = Renderer::GetInstance();
    for (const Run& run : runs_) {
        if (run.back != back) continue;
        RibbonTrail& trail = runPool_[run.poolIndex];
        // 奥側は、少し細く、薄くする(遠いので)。頂点カラーのアルファが、帯全体の濃さになる
        trail.SetWidth(kRibbonWidth * (back ? kRibbonBackWidthRatio : 1.0f));
        trail.SetColor({ 1.0f, 1.0f, 1.0f, alpha * (back ? kRibbonBackAlpha : 1.0f) });

        EnergyRibbonStyle style;
        style.glowColor = kOrbGlowColor;
        style.coreColor = kOrbCoreColor;
        style.outlineColor = kOrbOutlineColor;
        style.time = time;
        style.seed = seed_ + run.seed;
        style.intensity = kOrbIntensity * (1.0f + kRibbonChargeIntensity * charge);
        style.flowSpeed = kRibbonFlowSpeed;
        renderer->DrawEnergyRibbon(trail, style);
    }
}

void BossOrbVisualComponent::DrawAfterClouds() {
    if (!GetOwner() || !EnsureInitialized_()) return;
    const auto* orbComponent = GetOwner()->GetComponent<BossOrbComponent>();
    if (!orbComponent) return;

    const BossOrbLook& look = orbComponent->GetLook();
    // 消えるときは、全体が薄れる。まだ現れていないときは、何も描かない
    const float fade = 1.0f - look.dismiss;
    if (look.appear <= 0.0f || fade <= 0.0f) return;

    auto* renderer = Renderer::GetInstance();
    // 見た目の位置: GameObject の位置(当たり判定の位置)に、らせんのずれを足す
    const Vector3 position = {
        GetOwner()->GetTransform().translate.x + look.spiralOffset.x,
        GetOwner()->GetTransform().translate.y + look.spiralOffset.y,
        GetOwner()->GetTransform().translate.z + look.spiralOffset.z,
    };
    seed_ = look.seed;

    // --- 現れるときの輪: 玉の位置で、切れ目のある輪が広がりながら薄れる(現れきったら、消える) ---
    if (look.appear < 1.0f) {
        const float radius = Mix(kFlashRadiusStart, kFlashRadiusEnd, EaseOut(look.appear));
        ArcRing::Params ring;
        ring.innerRadius = radius - kFlashThickness * 0.5f;
        ring.outerRadius = radius + kFlashThickness * 0.5f;
        ring.arcCount = kFlashArcCount;
        ring.gapRatio = kFlashGapRatio;
        ring.rotation = look.age * kFlashSpinSpeed;
        ring.edgeColor = kOrbGlowColor;
        ring.coreColor = kOrbCoreColor;
        ring.intensity = kFlashIntensity;
        ring.alpha = std::pow(1.0f - look.appear, kFlashFadePower) * fade;
        flash_.SetParams(ring);
        flash_.SetBillboardTransform(position, kFlashQuadWorldRadius, renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
        renderer->DrawArcRing(&flash_);
    }

    // --- まとうリボンの形を作る(軌道は、現れるにつれて、玉のまわりへ広がる) ---
    Vector3 right{ 1.0f, 0.0f, 0.0f };
    Vector3 up{ 0.0f, 1.0f, 0.0f };
    Vector3 axis{ 0.0f, 0.0f, 1.0f };
    Vector3 cameraPosition{ 0.0f, 0.0f, 0.0f };
    GetCameraFrame(renderer->GetViewMatrix(), right, up, axis, cameraPosition);
    const float appearEased = EaseOut(look.appear);
    // まとうリボン: 構えの間は、軌道が縮み、速く回る(力を溜めている)。飛行中は、軌道が締まって、玉に付いてくる(漂って見えないように)
    const float orbit = appearEased * Mix(1.0f, kAimOrbitScale, look.aim) * Mix(1.0f, kFlightOrbitScale, look.flightBlend);
    BuildRibbons_(position, right, up, axis, cameraPosition, orbit, look.age, look.charge + kAimSpeedBoost * look.aim);

    // --- 飛んでいる間の尾: いちばん後ろに描く(玉と、まとうリボンが、尾の先端の上に重なる) ---
    if (look.flying) {
        trail_.SetColor({ 1.0f, 1.0f, 1.0f, fade });
        renderer->DrawEnergyRibbon(trail_, MakeTrailStyle_(look.age));
    }

    // --- 奥側のリボン -> 火の玉 -> 手前側のリボン、の順に描く(玉の後ろへ回った帯は、玉に隠れる) ---
    const float ribbonAlpha = appearEased * fade * Mix(1.0f, kFlightRibbonAlpha, look.flightBlend);
    DrawRuns_(true, ribbonAlpha, look.age, look.charge);

    EnergyOrb::Params params;
    params.bodyColor = kOrbBodyColor;
    params.glowColor = kOrbGlowColor;
    params.coreColor = kOrbCoreColor;
    params.outlineColor = kOrbOutlineColor;
    params.time = look.age;
    params.seed = look.seed;
    params.appear = look.appear;
    params.charge = look.charge;
    params.orbRadius = kOrbRadius;
    params.haloStrength = kOrbHaloStrength;
    params.intensity = kOrbIntensity * (1.0f + kAimIntensityBoost * look.aim);
    params.alpha = fade;
    params.flow = kOrbFlowDirection;
    if (look.flying) {
        // 飛んでいる間は、炎が、進む向きの逆へ流れ、火の玉が、進む向きへ伸びる(彗星のような形。画面の上での速さに応じて)。
        // 画面の上で向きが決まらないとき(視線に沿って飛ぶとき)は、上向きのまま、伸ばさない
        const Vector2 flow = { -Dot(look.direction, right), -Dot(look.direction, up) };
        const float flowLength = std::sqrt(flow.x * flow.x + flow.y * flow.y);
        if (flowLength >= kFlowMinLength) {
            params.flow = flow;
            params.stretch = kFlightStretch * std::clamp(flowLength, 0.0f, 1.0f) * look.flightBlend;
        }
    }
    orb_.SetParams(params);
    orb_.SetBillboardTransform(position, kOrbQuadWorldRadius, renderer->GetViewMatrix(), renderer->GetProjectionMatrix());
    renderer->DrawEnergyOrb(&orb_);

    DrawRuns_(false, ribbonAlpha, look.age, look.charge);
}
