#include "IslandOccluders.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kParallelEpsilon = 1e-6f; // 線分の高さの変化がこれより小さければ、水平とみなす

// 線分 from-to が、垂直な円柱(中心 cx, cz、半径 radius、高さ bottom から top)と交わるなら true。
// 手順: (1) 線分を、円柱の高さの範囲で切り取る(上下の面を含む)。(2) 切り取った線分を水平面へ落とし、円の中心との最短距離を半径と比べる。
// 線分が円柱の内側に入っていても、最短距離が半径以下になるので、true になる
bool SegmentHitsCylinder(const Vector3 &from, const Vector3 &to, float cx, float cz, float radius, float bottom, float top) {
  // (1) 高さの範囲で切り取る。線分上の位置を t(0 から 1)で表す
  float t0 = 0.0f;
  float t1 = 1.0f;
  const float dy = to.y - from.y;
  if (std::fabs(dy) < kParallelEpsilon) {
    // 水平な線分: 高さが範囲外なら、当たらない
    if (from.y < bottom || from.y > top) return false;
  } else {
    float ta = (bottom - from.y) / dy;
    float tb = (top - from.y) / dy;
    if (ta > tb) std::swap(ta, tb);
    t0 = std::max(t0, ta);
    t1 = std::min(t1, tb);
    if (t0 > t1) return false; // 高さの範囲を通らない
  }

  // (2) 切り取った線分の両端を、水平面(x, z)で求める
  const float ax = from.x + (to.x - from.x) * t0;
  const float az = from.z + (to.z - from.z) * t0;
  const float bx = from.x + (to.x - from.x) * t1;
  const float bz = from.z + (to.z - from.z) * t1;

  // 点(cx, cz)から、線分 a-b までの最短距離の 2 乗を求める(線分への射影を、端でクランプする)
  const float abx = bx - ax;
  const float abz = bz - az;
  const float lengthSq = abx * abx + abz * abz;
  float u = 0.0f;
  if (lengthSq > kParallelEpsilon) {
    u = std::clamp(((cx - ax) * abx + (cz - az) * abz) / lengthSq, 0.0f, 1.0f);
  }
  const float px = ax + abx * u - cx;
  const float pz = az + abz * u - cz;
  return px * px + pz * pz <= radius * radius;
}
} // namespace

void IslandOccluders::AddIsland(IslandModelKind kind, const Vector3 &position, float scale) {
  const IslandOccluderShape &shape = kIslandOccluderShapes[static_cast<size_t>(kind)];
  cylinders_.push_back({position.x, position.z, shape.radius * scale, position.y + shape.bottom * scale, position.y + shape.top * scale});
}

bool IslandOccluders::IsSegmentBlocked(const Vector3 &from, const Vector3 &to) const {
  // 線分の水平の外接の箱。島の数(30 ほど)でも、毎フレーム弾の数だけ呼ばれるので、
  // 円柱の外接の箱と重ならない島は、細かい計算の前に捨てる(ほとんどの島は、ここで終わる)
  const float minX = std::min(from.x, to.x);
  const float maxX = std::max(from.x, to.x);
  const float minZ = std::min(from.z, to.z);
  const float maxZ = std::max(from.z, to.z);
  const float minY = std::min(from.y, to.y);
  const float maxY = std::max(from.y, to.y);

  for (const Cylinder &c : cylinders_) {
    if (maxX < c.centerX - c.radius || minX > c.centerX + c.radius) continue;
    if (maxZ < c.centerZ - c.radius || minZ > c.centerZ + c.radius) continue;
    if (maxY < c.bottom || minY > c.top) continue;
    if (SegmentHitsCylinder(from, to, c.centerX, c.centerZ, c.radius, c.bottom, c.top)) return true;
  }
  return false;
}
