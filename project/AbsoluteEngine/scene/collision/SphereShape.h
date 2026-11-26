#pragma once
#include "CollisionShape.h"

namespace AbsoluteEngine {

// 球の当たり判定の形状
class SphereShape final : public ICollisionShape {
public:
  // radius: 半径(置くときの scale は、3 軸のうち最大のものを掛ける)
  explicit SphereShape(float radius) : radius_(radius) {}

  // 置いたときの半径(scale の 3 軸のうち最大のものを掛ける。球のまま大きさを変えるため)
  float GetScaledRadius(const ShapePlacement& placement) const;

  CollisionResult Collide(const ShapePlacement& self, const ICollisionShape& other, const ShapePlacement& otherPlacement) const override;
  CollisionResult CollideWith(const ShapePlacement& self, const SphereShape& first, const ShapePlacement& firstPlacement) const override;
  CollisionResult CollideWith(const ShapePlacement& self, const AabbShape& first, const ShapePlacement& firstPlacement) const override;
  bool Raycast(const ShapePlacement& self, const Vector3& origin, const Vector3& dir, float& outT) const override;
  void DrawWire(const ShapePlacement& self, const LineSink& drawLine) const override;

private:
  float radius_; // 半径
};

} // namespace AbsoluteEngine
