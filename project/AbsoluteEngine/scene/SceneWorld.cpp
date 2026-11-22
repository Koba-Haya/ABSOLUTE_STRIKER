#include "SceneWorld.h"

#include <algorithm>

#include "CollisionManager.h"
#include "GameObject.h"
#include "SceneSerializer.h"

namespace AbsoluteEngine {

void SceneWorld::Add(std::shared_ptr<GameObject> obj) {
  if (!obj) return;
  objects_.push_back(std::move(obj));
}

void SceneWorld::Remove(const std::shared_ptr<GameObject>& obj) {
  objects_.erase(std::remove(objects_.begin(), objects_.end(), obj), objects_.end());
}

void SceneWorld::RemoveIf(const std::function<bool(const std::shared_ptr<GameObject>&)>& shouldRemove) {
  objects_.erase(std::remove_if(objects_.begin(), objects_.end(), shouldRemove), objects_.end());
}

void SceneWorld::Clear() {
  objects_.clear();
}

void SceneWorld::Update(float deltaTime) {
  // 更新中に Add されても vector の再確保でループが壊れないよう、個数を固定して添字で回す
  const size_t count = objects_.size();
  for (size_t i = 0; i < count; ++i) {
    // 更新中の Add で要素が移動しても、この1回の Update の間は自分の分を保てるようコピーを持つ
    auto obj = objects_[i];
    if (obj && obj->IsActive()) {
      obj->Update(deltaTime);
    }
  }

  // 全オブジェクトの更新が終わってから、衝突判定を行う
  CollisionManager::GetInstance().Update(objects_);
}

void SceneWorld::CollectGarbage() {
  RemoveIf([](const std::shared_ptr<GameObject>& obj) {
    return !obj || !obj->IsActive();
  });
}

void SceneWorld::SaveSnapshot() {
  snapshotJson_ = SceneSerializer::SerializeToString(objects_, true);
}

bool SceneWorld::RestoreSnapshot() {
  objects_.clear();
  return SceneSerializer::DeserializeFromString(snapshotJson_, objects_);
}

bool SceneWorld::LoadFromFile(const std::string& filePath) {
  return SceneSerializer::Deserialize(filePath, objects_);
}

bool SceneWorld::SaveToFile(const std::string& filePath) const {
  return SceneSerializer::Serialize(filePath, objects_);
}

} // namespace AbsoluteEngine
