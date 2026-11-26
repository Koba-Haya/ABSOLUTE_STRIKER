#include "ShapePairTests.h"
#include "SphereShape.h"
#include "AabbShape.h"
#include <algorithm>
#include <cmath>

namespace AbsoluteEngine {

namespace ShapePairTests {

namespace {
// 中心どうしが、これより近いときは、向きを決められないので、決まった向きへ押し出す(0 割りを避ける)
constexpr float kMinSeparation = 0.0001f;
// 中心が重なったときに押し出す向き(上)
const Vector3 kFallbackNormal = {0.0f, 1.0f, 0.0f};

// 正なら +1、それ以外は -1(押し出す向きの符号)
float SignOf(float value) { return value > 0.0f ? 1.0f : -1.0f; }

// 3 軸のめり込み量のうち、最も小さい軸の向きへ押し出す結果を作る(最短の距離で重なりを解くため)。
// penetration: 軸ごとのめり込み量, delta: 相手から自分への中心の差(押し出す向きの符号に使う), extraDepth: 深さに足す量
CollisionResult PushOutAlongMinAxis(const Vector3& penetration, const Vector3& delta, float extraDepth) {
  CollisionResult result;
  result.isHit = true;
  if (penetration.x <= penetration.y && penetration.x <= penetration.z) {
    result.depth = penetration.x + extraDepth;
    result.normal = {SignOf(delta.x), 0.0f, 0.0f};
  } else if (penetration.y <= penetration.x && penetration.y <= penetration.z) {
    result.depth = penetration.y + extraDepth;
    result.normal = {0.0f, SignOf(delta.y), 0.0f};
  } else {
    result.depth = penetration.z + extraDepth;
    result.normal = {0.0f, 0.0f, SignOf(delta.z)};
  }
  return result;
}
} // namespace

CollisionResult SphereSphere(const SphereShape& a, const ShapePlacement& pa, const SphereShape& b, const ShapePlacement& pb) {
  CollisionResult result;
  const float dx = pa.center.x - pb.center.x;
  const float dy = pa.center.y - pb.center.y;
  const float dz = pa.center.z - pb.center.z;
  const float distSq = dx * dx + dy * dy + dz * dz;
  const float rSum = a.GetScaledRadius(pa) + b.GetScaledRadius(pb);

  if (distSq > rSum * rSum) {
    return result;
  }
  result.isHit = true;
  const float dist = std::sqrt(distSq);
  if (dist > kMinSeparation) {
    result.normal = {dx / dist, dy / dist, dz / dist};
    result.depth = rSum - dist;
  } else {
    result.normal = kFallbackNormal;
    result.depth = rSum;
  }
  return result;
}

CollisionResult AabbAabb(const AabbShape& a, const ShapePlacement& pa, const AabbShape& b, const ShapePlacement& pb) {
  const Vector3 delta = {pa.center.x - pb.center.x, pa.center.y - pb.center.y, pa.center.z - pb.center.z};
  const Vector3 ha = a.GetScaledHalfExtents(pa);
  const Vector3 hb = b.GetScaledHalfExtents(pb);
  const Vector3 penetration = {
      (ha.x + hb.x) - std::abs(delta.x),
      (ha.y + hb.y) - std::abs(delta.y),
      (ha.z + hb.z) - std::abs(delta.z)};

  // 3 軸とも重なっているときだけ当たり
  if (penetration.x > 0.0f && penetration.y > 0.0f && penetration.z > 0.0f) {
    return PushOutAlongMinAxis(penetration, delta, 0.0f);
  }
  return CollisionResult{};
}

CollisionResult SphereAabb(const SphereShape& sphere, const ShapePlacement& ps, const AabbShape& box, const ShapePlacement& pb) {
  CollisionResult result;
  const float radius = sphere.GetScaledRadius(ps);
  const Vector3 half = box.GetScaledHalfExtents(pb);
  const Vector3& s = ps.center;
  const Vector3& c = pb.center;

  // 箱の上で、球の中心に最も近い点
  const float closestX = std::max(c.x - half.x, std::min(s.x, c.x + half.x));
  const float closestY = std::max(c.y - half.y, std::min(s.y, c.y + half.y));
  const float closestZ = std::max(c.z - half.z, std::min(s.z, c.z + half.z));

  const float dx = s.x - closestX;
  const float dy = s.y - closestY;
  const float dz = s.z - closestZ;
  const float distSq = dx * dx + dy * dy + dz * dz;

  if (distSq > radius * radius) {
    return result;
  }
  const float dist = std::sqrt(distSq);
  if (dist > kMinSeparation) {
    result.isHit = true;
    result.normal = {dx / dist, dy / dist, dz / dist};
    result.depth = radius - dist;
    return result;
  }

  // 球の中心が箱の内部にある場合は、中心から一番近い面へ押し出す
  const Vector3 delta = {s.x - c.x, s.y - c.y, s.z - c.z};
  const Vector3 toFace = {half.x - std::abs(delta.x), half.y - std::abs(delta.y), half.z - std::abs(delta.z)};
  return PushOutAlongMinAxis(toFace, delta, radius);
}

CollisionResult Flip(CollisionResult result) {
  result.normal = {-result.normal.x, -result.normal.y, -result.normal.z};
  return result;
}

} // namespace ShapePairTests

} // namespace AbsoluteEngine
