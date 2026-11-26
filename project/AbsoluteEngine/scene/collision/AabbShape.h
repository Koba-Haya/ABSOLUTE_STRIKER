#pragma once
#include "CollisionShape.h"

namespace AbsoluteEngine {

// 軸に沿った箱(AABB)の当たり判定の形状
class AabbShape final : public ICollisionShape {
public:
  // halfExtents: 中心から各面までの距離(半幅)。置くときの scale を軸ごとに掛ける
  explicit AabbShape(const Vector3& halfExtents) : halfExtents_(halfExtents) {}

  // 置いたときの半幅(scale を軸ごとに掛ける)
  Vector3 GetScaledHalfExtents(const ShapePlacement& placement) const;

  CollisionResult Collide(const ShapePlacement& self, const ICollisionShape& other, const ShapePlacement& otherPlacement) const override;
  CollisionResult CollideWith(const ShapePlacement& self, const SphereShape& first, const ShapePlacement& firstPlacement) const override;
  CollisionResult CollideWith(const ShapePlacement& self, const AabbShape& first, const ShapePlacement& firstPlacement) const override;
  bool Raycast(const ShapePlacement& self, const Vector3& origin, const Vector3& dir, float& outT) const override;
  void DrawWire(const ShapePlacement& self, const LineSink& drawLine) const override;

private:
  Vector3 halfExtents_; // 半幅
};

} // namespace AbsoluteEngine
