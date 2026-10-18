#include "RibbonTrailFadeComponent.h"
#include "Renderer.h"

void RibbonTrailFadeComponent::Initialize(const RibbonTrail& trail) {
    trail_ = trail;
}

void RibbonTrailFadeComponent::Update(float deltaTime) {
    if (!owner_) return;

    // 新しい点は記録せず、点を古くするだけ。寿命の割合で先細りとフェードが進み、尾が縮んで消える
    trail_.Fade(deltaTime);

    // 帯を作れなくなったら、尾は消えきった。GameObject ごと破棄する(実際の削除はシーンが行う)
    if (!trail_.HasRibbon()) {
        owner_->Destroy();
    }
}

void RibbonTrailFadeComponent::Draw() {
    Renderer::GetInstance()->DrawRibbon(trail_);
}
