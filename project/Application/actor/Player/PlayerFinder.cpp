#include "PlayerFinder.h"

#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "PlayerComponent.h"

namespace PlayerFinder {

std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer(const AbsoluteEngine::ISceneQuery& query) {
  for (const auto& obj : query.Objects()) {
    if (obj && obj->GetComponent<PlayerComponent>()) {
      return obj;
    }
  }
  return nullptr;
}

} // namespace PlayerFinder
