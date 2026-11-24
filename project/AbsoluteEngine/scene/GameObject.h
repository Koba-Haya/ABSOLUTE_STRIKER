#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Transform.h"
#include "Component.h"
#include "../graphics/3d/model/ModelInstance.h"

namespace AbsoluteEngine {

// Removed hardcoded structs (ColliderInfo, LightComponent, DissolveInfo)
// These are now handled by respective component classes.

class GameObject : public std::enable_shared_from_this<GameObject> {
public:
  GameObject(const std::string& name = "GameObject");
  virtual ~GameObject() = default;

  // 基本プロパティ
  void SetName(const std::string& name) { name_ = name; }
  const std::string& GetName() const { return name_; }

  // --- Transform ---
  // 読み取り専用。書き換えは、下の Set 系 / Translate を使う(書き込み口をこのクラスに集めるため)
  const Transform& GetTransform() const { return transform_; }

  // 位置を設定する
  void SetPosition(const Vector3& position) { transform_.translate = position; }
  // 回転(ラジアン、オイラー角)を設定する
  void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
  // 拡大率を設定する
  void SetScale(const Vector3& scale) { transform_.scale = scale; }
  // 位置・回転・拡大をまとめて設定する
  void SetTransform(const Transform& transform) { transform_ = transform; }
  // 現在の位置に delta を加える(押し戻しなど、差分で動かすとき用)
  void Translate(const Vector3& delta) {
    transform_.translate.x += delta.x;
    transform_.translate.y += delta.y;
    transform_.translate.z += delta.z;
  }

  // 親子関係
  void AddChild(std::shared_ptr<GameObject> child);
  void RemoveChild(std::shared_ptr<GameObject> child);
  
  std::shared_ptr<GameObject> GetParent() const { return parent_.lock(); }
  const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return children_; }

  // 毎フレームの更新（コンポーネントと子ノードの更新）
  virtual void Update(float deltaTime);

  // コンポーネント管理
  void AddComponent(std::unique_ptr<IComponent> component);
  void RemoveComponent(IComponent* component);
  const std::vector<std::unique_ptr<IComponent>>& GetComponents() const { return components_; }

  template <typename T>
  T* GetComponent() const {
    for (auto& comp : components_) {
      if (T* t = dynamic_cast<T*>(comp.get())) {
        return t;
      }
    }
    return nullptr;
  }

  template <typename T>
  bool HasComponent() const {
    return GetComponent<T>() != nullptr;
  }

  // オブジェクトと子ノードの描画
  virtual void Draw();

  // メタデータプロパティ
  const std::string& GetTag() const { return tag_; }
  void SetTag(const std::string& tag) { tag_ = tag; }

  // プレハブパス
  const std::string& GetPrefabPath() const { return prefabPath_; }
  void SetPrefabPath(const std::string& path) { prefabPath_ = path; }
  bool IsPrefabInstance() const { return !prefabPath_.empty(); }

  // 生存フラグ
  bool IsActive() const { return isActive_; }
  void Destroy() { isActive_ = false; }

  // タイムラインプレビューフラグ（タスク16）
  // true の場合はシリアライザ（SceneSerializer）が保存対象から除外する
  bool IsTimelinePreview() const { return isTimelinePreview_; }
  void SetTimelinePreview(bool flag) { isTimelinePreview_ = flag; }

private:
  std::string name_;
  std::string tag_ = "Untagged";
  std::string prefabPath_ = "";

  bool isActive_ = true;

  // タイムラインプレビューフラグ（タスク16）
  // true の場合はシリアライザが JSON への保存対象から除外する
  bool isTimelinePreview_ = false;

  Transform transform_;

  std::weak_ptr<GameObject> parent_;
  std::vector<std::shared_ptr<GameObject>> children_;
  std::vector<std::unique_ptr<IComponent>> components_;
};

} // namespace AbsoluteEngine
