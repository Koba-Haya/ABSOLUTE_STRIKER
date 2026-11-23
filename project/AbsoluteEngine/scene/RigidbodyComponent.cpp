#include "RigidbodyComponent.h"
#include "GameObject.h"
#include "Transform.h"

#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"
#endif

namespace AbsoluteEngine {

void RigidbodyComponent::Serialize(nlohmann::json& j) const {
    j["mass"] = mass_;
    j["drag"] = drag_;
    j["useGravity"] = useGravity_;
    j["isKinematic"] = isKinematic_;
    j["velocity"] = {velocity_.x, velocity_.y, velocity_.z};
}

void RigidbodyComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("mass")) mass_ = j["mass"].get<float>();
    if (j.contains("drag")) drag_ = j["drag"].get<float>();
    if (j.contains("useGravity")) useGravity_ = j["useGravity"].get<bool>();
    if (j.contains("isKinematic")) isKinematic_ = j["isKinematic"].get<bool>();
    if (j.contains("velocity")) {
        velocity_.x = j["velocity"][0].get<float>();
        velocity_.y = j["velocity"][1].get<float>();
        velocity_.z = j["velocity"][2].get<float>();
    }
}

void RigidbodyComponent::AddForce(const Vector3& force) {
    if (mass_ <= 0.0f) return;
    acceleration_.x += force.x / mass_;
    acceleration_.y += force.y / mass_;
    acceleration_.z += force.z / mass_;
}

void RigidbodyComponent::Update(float deltaTime) {
    if (isKinematic_ || !GetOwner()) return;

    if (useGravity_) {
        acceleration_.y += GRAVITY;
    }

    velocity_.x += acceleration_.x * deltaTime;
    velocity_.y += acceleration_.y * deltaTime;
    velocity_.z += acceleration_.z * deltaTime;

    // 空気抵抗 (簡易的な減衰)
    velocity_.x *= (1.0f - drag_ * deltaTime);
    velocity_.y *= (1.0f - drag_ * deltaTime);
    velocity_.z *= (1.0f - drag_ * deltaTime);

    // 加速度のリセット
    acceleration_ = {0.0f, 0.0f, 0.0f};

    // Transform に反映
    auto& transform = GetOwner()->GetTransform();
    transform.translate.x += velocity_.x * deltaTime;
    transform.translate.y += velocity_.y * deltaTime;
    transform.translate.z += velocity_.z * deltaTime;
}

void RigidbodyComponent::DrawInspectorUI() {
#ifdef USE_IMGUI
    ImGui::PushID(this);
    
    ImGui::Checkbox("Is Kinematic", &isKinematic_);
    ImGui::Checkbox("Use Gravity", &useGravity_);
    ImGui::DragFloat("Mass", &mass_, 0.1f, 0.001f, 1000.0f);
    ImGui::DragFloat("Drag", &drag_, 0.01f, 0.0f, 100.0f);

    float v[3] = {velocity_.x, velocity_.y, velocity_.z};
    if (ImGui::DragFloat3("Velocity", v, 0.1f)) {
        velocity_ = {v[0], v[1], v[2]};
    }

    ImGui::PopID();
#endif
}

} // namespace AbsoluteEngine
