#include "SceneNavigator.h"

void SceneNavigator::RequestChange(const std::string &sceneId) {
  if (changer_) {
    changer_->RequestChange(sceneId);
  }
}

void SceneNavigator::RequestChange(const std::string &sceneId, std::any payload) {
  if (changer_) {
    changer_->RequestChange(sceneId, std::move(payload));
  }
}

void SceneNavigator::RequestChangeKeepingPayload(const std::string &sceneId) {
  RequestChange(sceneId, payload_);
}

bool SceneNavigator::IsTransitioning() const {
  return changer_ != nullptr && changer_->IsTransitioning();
}

float SceneNavigator::GetRevealProgress() const {
  return changer_ != nullptr ? changer_->GetRevealProgress() : 1.0f;
}

float SceneNavigator::GetRevealSeconds() const {
  return changer_ != nullptr ? changer_->GetRevealSeconds() : 0.0f;
}
