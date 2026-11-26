#include "AabbShape.h"
#include "SphereShape.h"
#include "ShapePairTests.h"
#include <algorithm>

namespace AbsoluteEngine {

namespace {
// レイの向きの成分が 0 のときに、代わりに割る値(0 割りを避ける。以前のエディタのコードと同じ値)
constexpr float kNonZeroDirection = 0.00001f;
// 箱の 12 本の辺(角の番号の組)。角の番号は DrawWire の corners の並び
constexpr int kEdgeCount = 12;
constexpr int kEdges[kEdgeCount][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // 手前の面(z が小さい側)
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // 奥の面(z が大きい側)
    {0, 4}, {1, 5}, {2, 6}, {3, 7}, // 手前と奥をつなぐ辺
};

// レイの向きの 1 成分で割るための値(0 なら、ごく小さい値にする)
float SafeDirection(float d) { return d != 0.0f ? d : kNonZeroDirection; }
} // namespace

Vector3 AabbShape::GetScaledHalfExtents(const ShapePlacement& placement) const {
  return {halfExtents_.x * placement.scale.x, halfExtents_.y * placement.scale.y, halfExtents_.z * placement.scale.z};
}

CollisionResult AabbShape::Collide(const ShapePlacement& self, const ICollisionShape& other, const ShapePlacement& otherPlacement) const {
  // 1 回目の振り分けで、自分が箱だと決まった。相手に「箱(自分)との判定」を頼む(2 回目の振り分けで、相手の型が決まる)
  return other.CollideWith(otherPlacement, *this, self);
}

CollisionResult AabbShape::CollideWith(const ShapePlacement& self, const SphereShape& first, const ShapePlacement& firstPlacement) const {
  // 球(first)と箱(自分)。計算は球(first)を押し出す向きで返るので、そのまま
  return ShapePairTests::SphereAabb(first, firstPlacement, *this, self);
}

CollisionResult AabbShape::CollideWith(const ShapePlacement& self, const AabbShape& first, const ShapePlacement& firstPlacement) const {
  // 箱(first)と箱(自分)
  return ShapePairTests::AabbAabb(first, firstPlacement, *this, self);
}

bool AabbShape::Raycast(const ShapePlacement& self, const Vector3& origin, const Vector3& dir, float& outT) const {
  // スラブ法: 軸ごとに、レイが箱の 2 枚の面の間にいる区間を求め、3 軸の区間が重なれば当たり
  const Vector3 half = GetScaledHalfExtents(self);
  const Vector3 minBounds = {self.center.x - half.x, self.center.y - half.y, self.center.z - half.z};
  const Vector3 maxBounds = {self.center.x + half.x, self.center.y + half.y, self.center.z + half.z};

  const float t1 = (minBounds.x - origin.x) / SafeDirection(dir.x);
  const float t2 = (maxBounds.x - origin.x) / SafeDirection(dir.x);
  const float t3 = (minBounds.y - origin.y) / SafeDirection(dir.y);
  const float t4 = (maxBounds.y - origin.y) / SafeDirection(dir.y);
  const float t5 = (minBounds.z - origin.z) / SafeDirection(dir.z);
  const float t6 = (maxBounds.z - origin.z) / SafeDirection(dir.z);

  const float tmin = (std::max)((std::max)((std::min)(t1, t2), (std::min)(t3, t4)), (std::min)(t5, t6));
  const float tmax = (std::min)((std::min)((std::max)(t1, t2), (std::max)(t3, t4)), (std::max)(t5, t6));

  // 箱がレイの後ろにある、または区間が重ならないときは外れ
  if (tmax < 0.0f || tmin > tmax) {
    return false;
  }
  outT = tmin;
  return true;
}

void AabbShape::DrawWire(const ShapePlacement& self, const LineSink& drawLine) const {
  // 8 つの角を求め、12 本の辺を描く
  const Vector3 h = GetScaledHalfExtents(self);
  const Vector3& c = self.center;
  const Vector3 corners[8] = {
      {c.x - h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y - h.y, c.z - h.z},
      {c.x + h.x, c.y + h.y, c.z - h.z}, {c.x - h.x, c.y + h.y, c.z - h.z},
      {c.x - h.x, c.y - h.y, c.z + h.z}, {c.x + h.x, c.y - h.y, c.z + h.z},
      {c.x + h.x, c.y + h.y, c.z + h.z}, {c.x - h.x, c.y + h.y, c.z + h.z},
  };
  for (const auto& edge : kEdges) {
    drawLine(corners[edge[0]], corners[edge[1]]);
  }
}

} // namespace AbsoluteEngine
