#include "MuzzleOrbGlow.h"

#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/ModelComponent.h"

namespace {
// 光る玉のモデルとテクスチャ(exe 基準)。球は半径 1(以前 TelegraphComponent にあった値と同じ)
constexpr const char* kOrbModelPath = "resources/app/sphere/sphere.obj";
constexpr const char* kOrbTexturePath = "resources/app/cube/white100x100.png";
constexpr const char* kOrbObjectName = "TelegraphGlow";
constexpr const char* kOrbTag = "TelegraphGlow";
// 敵の拡大率の平均を求めるときの軸の数
constexpr float kAxisCount = 3.0f;
} // namespace

MuzzleOrbGlow::~MuzzleOrbGlow() {
    DestroyOrb_();
}

void MuzzleOrbGlow::CreateOrb_() {
    if (orb_.lock()) return;

    auto orb = std::make_shared<AbsoluteEngine::GameObject>(kOrbObjectName);
    orb->SetTag(kOrbTag);
    auto model = std::make_unique<AbsoluteEngine::ModelComponent>();
    model->SetLightingMode(ModelLightingMode::kUnlit); // 照明なし(光って見える)
    model->LoadModel(kOrbModelPath);
    model->LoadTexture(kOrbTexturePath);
    model->SetColor(settings_.glowColor);
    orb->AddComponent(std::move(model));
    spawner_.Spawn(orb);
    orb_ = orb;
}

void MuzzleOrbGlow::DestroyOrb_() {
    if (auto orb = orb_.lock()) {
        orb->Destroy();
    }
    orb_.reset();
}

void MuzzleOrbGlow::OnBegin(AbsoluteEngine::GameObject& owner, float progress) {
    CreateOrb_();
    OnUpdate(owner, progress);
}

void MuzzleOrbGlow::OnUpdate(AbsoluteEngine::GameObject& owner, float progress) {
    auto orb = orb_.lock();
    if (!orb) return;

    const auto& ownerTransform = owner.GetTransform();
    const Vector3& s = ownerTransform.scale;
    // 出口の位置: 敵の位置 + 出口のずれ x 敵の拡大率(敵の回転は見ない。今の敵は、回転せずに出る)
    orb->SetPosition({ ownerTransform.translate.x + settings_.muzzleOffset.x * s.x,
                       ownerTransform.translate.y + settings_.muzzleOffset.y * s.y,
                       ownerTransform.translate.z + settings_.muzzleOffset.z * s.z });
    // 大きさ: 始めから終わりへ、なめらかに(終わりに近づくほど速く膨らむ)。敵の拡大率の平均を掛ける
    const float eased = progress * progress;
    const float orbScale = (settings_.orbScaleStart + (settings_.orbScaleEnd - settings_.orbScaleStart) * eased) *
                           ((s.x + s.y + s.z) / kAxisCount);
    orb->SetScale({ orbScale, orbScale, orbScale });
}

void MuzzleOrbGlow::OnEnd(AbsoluteEngine::GameObject* owner, const Vector4& baseColor) {
    // 体の色は替えていないので、元に戻すものは無い。光る玉を消すだけ
    (void)owner;
    (void)baseColor;
    DestroyOrb_();
}
