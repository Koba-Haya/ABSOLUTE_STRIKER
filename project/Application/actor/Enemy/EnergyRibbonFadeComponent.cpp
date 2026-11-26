#include "EnergyRibbonFadeComponent.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/scene/GameObject.h"

void EnergyRibbonFadeComponent::Initialize(const RibbonTrail& trail, const EnergyRibbonStyle& style) {
    trail_ = trail;
    style_ = style;
}

void EnergyRibbonFadeComponent::Update(float deltaTime) {
    if (!GetOwner()) return;

    // 新しい点は記録せず、点を古くするだけ。寿命の割合で先細りとフェードが進み、尾が縮んで消える。模様の流れも、止めずに進める
    trail_.Fade(deltaTime);
    style_.time += deltaTime;

    // 帯を作れなくなったら、尾は消えきった。GameObject ごと破棄する(実際の削除はシーンが行う)
    if (!trail_.HasRibbon()) {
        GetOwner()->Destroy();
    }
}

void EnergyRibbonFadeComponent::DrawAfterClouds() {
    Renderer::GetInstance()->DrawEnergyRibbon(trail_, style_);
}
