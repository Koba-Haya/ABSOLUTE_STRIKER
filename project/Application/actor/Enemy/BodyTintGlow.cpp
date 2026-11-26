#include "BodyTintGlow.h"

#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ModelComponent.h"

void BodyTintGlow::OnBegin(AbsoluteEngine::GameObject& owner, float progress) {
    // 色は、予備動作の間ずっと同じ(進み具合では変えない)
    (void)progress;
    if (auto* model = owner.GetComponent<AbsoluteEngine::ModelComponent>()) {
        model->SetColor(tint_);
    }
}

void BodyTintGlow::OnEnd(AbsoluteEngine::GameObject* owner, const Vector4& baseColor) {
    // 体の色を、予備動作の前の色へ戻す
    if (!owner) return;
    if (auto* model = owner->GetComponent<AbsoluteEngine::ModelComponent>()) {
        model->SetColor(baseColor);
    }
}
