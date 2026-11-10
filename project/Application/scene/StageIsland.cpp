#include "StageIsland.h"

#include "Method.h"
#include "ModelResource.h"
#include "Renderer.h"

namespace {
constexpr int32_t kHalfLambert = 2; // 光の当たらない側も真っ黒にしない照明モード(ModelLightingMode::kHalfLambert)
} // namespace

bool StageIsland::Initialize(const std::shared_ptr<ModelResource> &resource) {
  if (!resource) {
    return false;
  }
  ModelInstance::CreateInfo info{};
  info.resource = resource;
  info.lightingMode = kHalfLambert;
  info.specularColor = {0.0f, 0.0f, 0.0f}; // 岩と草は、つやなし
  return model_.Initialize(info);
}

void StageIsland::SetPlacement(const Vector3 &position, float scale, const Vector3 &rotate) {
  position_ = position;
  scale_ = scale;
  rotate_ = rotate;
}

void StageIsland::Draw(Renderer *renderer) {
  // Quaternion 版と区別するため、オイラー角を Vector3 で渡す
  model_.SetWorld(MakeAffineMatrix({scale_, scale_, scale_}, rotate_, position_));
  renderer->DrawModel(&model_);
}
