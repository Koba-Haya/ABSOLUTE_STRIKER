#include "ItemSpawner.h"

#include <memory>
#include <utility>

#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "ItemPickupComponent.h"

namespace {
constexpr const char *kItemObjectName = "Item";
constexpr const char *kItemTag = "Item";
} // namespace

void ItemSpawner::Spawn(BaseScene &scene, const ItemDefinition &item, const ItemPickupSettings &settings, const Vector3 &position,
                        std::function<void()> onCollected) {
  auto obj = std::make_shared<AbsoluteEngine::GameObject>(kItemObjectName);
  obj->SetTag(kItemTag);
  obj->GetTransform().translate = position;
  obj->GetTransform().scale = {item.scale, item.scale, item.scale};

  // 見た目: 照明なし(パレットの色をそのまま出す)。落ちている強化が、背景の中で光って見えるようにする
  auto model = std::make_unique<AbsoluteEngine::ModelComponent>();
  model->SetLightingMode(ModelLightingMode::kUnlit);
  model->LoadModel(item.modelPath);
  model->LoadTexture(item.texturePath);
  obj->AddComponent(std::move(model));

  // 動きと取得
  auto pickup = std::make_unique<ItemPickupComponent>();
  pickup->Configure(settings, item.scale, std::move(onCollected));
  obj->AddComponent(std::move(pickup));

  scene.AddRootObject(obj);
}
