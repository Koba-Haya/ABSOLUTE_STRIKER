#include "SphereShape.h"
#include "AabbShape.h"
#include "ShapePairTests.h"
#include "Method.h"
#include <algorithm>
#include <cmath>

namespace AbsoluteEngine {

float SphereShape::GetScaledRadius(const ShapePlacement& placement) const {
  const float maxScale = (std::max)({placement.scale.x, placement.scale.y, placement.scale.z});
  return radius_ * maxScale;
}

CollisionResult SphereShape::Collide(const ShapePlacement& self, const ICollisionShape& other, const ShapePlacement& otherPlacement) const {
  // 1 回目の振り分けで、自分が球だと決まった。相手に「球(自分)との判定」を頼む(2 回目の振り分けで、相手の型が決まる)
  return other.CollideWith(otherPlacement, *this, self);
}

CollisionResult SphereShape::CollideWith(const ShapePlacement& self, const SphereShape& first, const ShapePlacement& firstPlacement) const {
  // 球(first)と球(自分)
  return ShapePairTests::SphereSphere(first, firstPlacement, *this, self);
}

CollisionResult SphereShape::CollideWith(const ShapePlacement& self, const AabbShape& first, const ShapePlacement& firstPlacement) const {
  // 箱(first)と球(自分)。計算は球を押し出す向きで返るので、箱(first)を押し出す向きへ反転する
  return ShapePairTests::Flip(ShapePairTests::SphereAabb(*this, self, first, firstPlacement));
}

bool SphereShape::Raycast(const ShapePlacement& self, const Vector3& origin, const Vector3& dir, float& outT) const {
  // レイと球の交点(2 次方程式の判別式)。手前の交点がレイの前方にあるときだけ当たり
  const float radius = GetScaledRadius(self);
  const Vector3 m = {origin.x - self.center.x, origin.y - self.center.y, origin.z - self.center.z};
  const float b = Dot(m, dir);
  const float c = Dot(m, m) - radius * radius;
  const float discriminant = b * b - c;
  if (discriminant <= 0.0f) {
    return false;
  }
  const float t = -b - std::sqrt(discriminant);
  if (t <= 0.0f) {
    return false;
  }
  outT = t;
  return true;
}

void SphereShape::DrawWire(const ShapePlacement& self, const LineSink& drawLine) const {
  // 3 軸それぞれの向きに、直径の線分を描く(球の大きさが分かる、軽い表示)
  const float r = GetScaledRadius(self);
  const Vector3& c = self.center;
  drawLine({c.x - r, c.y, c.z}, {c.x + r, c.y, c.z});
  drawLine({c.x, c.y - r, c.z}, {c.x, c.y + r, c.z});
  drawLine({c.x, c.y, c.z - r}, {c.x, c.y, c.z + r});
}

} // namespace AbsoluteEngine
