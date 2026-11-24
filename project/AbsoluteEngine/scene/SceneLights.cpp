#include "SceneLights.h"

#include <functional>
#include <memory>
#include <vector>

#include "GameObject.h"
#include "LightNodeComponent.h"
#include "Renderer.h"
#include "SceneWorld.h"

namespace AbsoluteEngine {

namespace {
// ライトが何も向いていないときの基準の向き(真下)。Transform の回転をこの向きに掛けて、ライトの向きを求める
const Vector3 kDefaultLightDirection = {0.0f, -1.0f, 0.0f};
} // namespace

void SceneLights::ApplyToRenderer(const SceneWorld& world, Renderer* renderer) {
  if (!renderer) return;

  std::vector<DirLight> dirLights;
  std::vector<PointLight> pointLights;
  std::vector<SpotLight> spotLights;

  // 子オブジェクトにもライトが付いていることがあるため、木を再帰でたどって集める
  auto collectLights = [&](auto& self, const std::shared_ptr<GameObject>& obj) -> void {
    if (!obj) return;
    const auto& t = obj->GetTransform();

    auto lightComp = obj->GetComponent<LightNodeComponent>();
    if (lightComp && lightComp->GetSettings().type != LightNodeComponent::Type::None) {
      const auto& light = lightComp->GetSettings();

      // Transform の回転から方向ベクトルを計算
      Matrix4x4 rotX = MakeRotateXMatrix(t.rotate.x);
      Matrix4x4 rotY = MakeRotateYMatrix(t.rotate.y);
      Matrix4x4 rotZ = MakeRotateZMatrix(t.rotate.z);
      Matrix4x4 rotMatrix = Multiply(Multiply(rotZ, rotX), rotY);

      Vector3 dir = TransformNormal(kDefaultLightDirection, rotMatrix);
      dir = Normalize(dir);

      if (light.type == LightNodeComponent::Type::Directional) {
        DirLight dl;
        dl.color = light.color;
        dl.intensity = light.intensity;
        dl.direction = dir;
        dl.enabled = true;
        dirLights.push_back(dl);
      } else if (light.type == LightNodeComponent::Type::Point) {
        PointLight pl;
        pl.color = light.color;
        pl.intensity = light.intensity;
        pl.radius = light.radius;
        pl.decay = light.decay;
        pl.position = t.translate;
        pl.enabled = true;
        pointLights.push_back(pl);
      } else if (light.type == LightNodeComponent::Type::Spot) {
        SpotLight sl;
        sl.color = light.color;
        sl.intensity = light.intensity;
        sl.distance = light.distance;
        sl.decay = light.decay;
        sl.coneAngleDeg = light.coneAngleDeg;
        sl.position = t.translate;
        sl.direction = dir;
        sl.enabled = true;
        spotLights.push_back(sl);
      }
    }

    for (const auto& child : obj->GetChildren()) {
      self(self, child);
    }
  };

  for (const auto& obj : world.Objects()) {
    collectLights(collectLights, obj);
  }

  renderer->SetDirectionalLights(dirLights, true);
  renderer->SetPointLights(pointLights, true);
  renderer->SetSpotLights(spotLights, true);
}

} // namespace AbsoluteEngine
