#include "RibbonTrailComponent.h"
#include "RibbonTrailFadeComponent.h"
#include "Renderer.h"
#include "../../../AbsoluteEngine/scene/BaseScene.h"
#include <memory>

namespace {
// 切り離した尾の GameObject の名前
constexpr const char* kFadeObjectName = "RibbonTrailFade";
} // namespace

void RibbonTrailComponent::Initialize(const RibbonTrail::Params& params) {
    trail_.Initialize(params);
}

void RibbonTrailComponent::Update(float deltaTime) {
    if (!owner_) return;

    // owner の現在位置を渡す。点を記録するかどうかは RibbonTrail 側が距離で判断する
    trail_.Update(deltaTime, owner_->GetTransform().translate);
}

void RibbonTrailComponent::DetachToScene() {
    auto* scene = BaseScene::GetActiveScene();
    if (!scene) return;

    // 先頭位置を最後の点として確定する。以後、この軌跡は新しい点を記録しない
    trail_.Freeze();
    if (!trail_.HasRibbon()) return; // 点が少なくて帯にならないなら、残す意味がない

    // 尾を別の GameObject へ引き渡す。コピーを渡し、こちらは空にして二重に描かれないようにする
    auto fadeObj = std::make_shared<AbsoluteEngine::GameObject>(kFadeObjectName);
    auto fadeComp = std::make_unique<RibbonTrailFadeComponent>();
    fadeComp->Initialize(trail_);
    fadeObj->AddComponent(std::move(fadeComp));
    scene->AddRootObject(fadeObj);

    trail_.Clear();
}

void RibbonTrailComponent::Draw() {
    // 帯の頂点生成と描画は、Renderer と RibbonDrawer が行う
    Renderer::GetInstance()->DrawRibbon(trail_);
}
