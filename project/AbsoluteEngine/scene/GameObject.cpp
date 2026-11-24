#include "GameObject.h"
#include <algorithm>
#include "../graphics/3d/model/ModelManager.h"
#include "../graphics/texture/TextureManager.h"

namespace AbsoluteEngine {

GameObject::GameObject(const std::string& name) : name_(name) {
}

// Removed SetEnvironmentCoefficient

void GameObject::AddChild(std::shared_ptr<GameObject> child) {
  if (child) {
    child->parent_ = shared_from_this();
    children_.push_back(child);
  }
}

void GameObject::RemoveChild(std::shared_ptr<GameObject> child) {
  auto it = std::find(children_.begin(), children_.end(), child);
  if (it != children_.end()) {
    (*it)->parent_.reset();
    children_.erase(it);
  }
}

void GameObject::AddComponent(std::unique_ptr<IComponent> component) {
  if (component) {
    component->SetOwner(this);
    components_.push_back(std::move(component));
  }
}

void GameObject::RemoveComponent(IComponent* component) {
  auto it = std::remove_if(components_.begin(), components_.end(),
      [component](const std::unique_ptr<IComponent>& ptr) {
          return ptr.get() == component;
      });
  if (it != components_.end()) {
      components_.erase(it, components_.end());
  }
}

void GameObject::Update(float deltaTime) {
  for (auto& comp : components_) {
    comp->Update(deltaTime);
  }
  for (auto& child : children_) {
    child->Update(deltaTime);
  }
}

void GameObject::Draw() {
  for (auto& comp : components_) {
    comp->Draw();
  }

  // 子のDrawを呼ぶ
  for (auto& child : children_) {
    child->Draw();
  }
}

void GameObject::DrawAfterClouds() {
  for (auto& comp : components_) {
    comp->DrawAfterClouds();
  }
  for (auto& child : children_) {
    child->DrawAfterClouds();
  }
}

// Removed LoadModel and LoadTexture

} // namespace AbsoluteEngine
