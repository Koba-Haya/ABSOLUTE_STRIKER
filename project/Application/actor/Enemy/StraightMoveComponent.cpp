#include "StraightMoveComponent.h"

void StraightMoveComponent::Update(float deltaTime) {
    if (owner_) {
        // direction_ の向きに移動する(既定は Z 軸の手前方向 = マイナス方向)
        auto& t = owner_->GetTransform();
        t.translate.x += direction_.x * speed_ * deltaTime;
        t.translate.y += direction_.y * speed_ * deltaTime;
        t.translate.z += direction_.z * speed_ * deltaTime;
    }
}

void StraightMoveComponent::OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) {
    // レールの進行方向(奥へ向かう向き)の逆 = 自機側へ近づく向き
    direction_ = { -frame.forward.x, -frame.forward.y, -frame.forward.z };
}
