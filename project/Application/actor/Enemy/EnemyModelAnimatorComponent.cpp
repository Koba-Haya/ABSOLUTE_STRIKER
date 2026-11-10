#include "EnemyModelAnimatorComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ModelComponent.h"

namespace {
constexpr const char* kJsonKeyAnimation = "animation";
}

void EnemyModelAnimatorComponent::Update(float /*deltaTime*/) {
    if (started_ || !owner_) return;

    // モデルが無い(まだ付いていない)場合は、次のフレームでもう一度試す
    auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>();
    if (!modelComp) return;

    modelComp->PlayAnimation(animation_, true);
    started_ = true;
}

void EnemyModelAnimatorComponent::Serialize(nlohmann::json& j) const {
    j[kJsonKeyAnimation] = animation_;
}

void EnemyModelAnimatorComponent::Deserialize(const nlohmann::json& j) {
    animation_ = j.value(kJsonKeyAnimation, std::string(kDefaultAnimation));
}
