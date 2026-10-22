#include "VoronoiFracture.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr float kTwoPi = 6.2831853f;
// 種を置く試行の上限(1 つの種あたり)。条件に合わない場所を引き続けても、止まるようにする
constexpr int kMaxAttemptsPerSeed = 30;
// 「真ん中の線の上」とみなす誤差。これ以下の点は、どちらの側にも入れる(切り口の頂点が重複して増えないようにする)
constexpr float kPlaneEpsilon = 1e-6f;
// 2 つの種が同じ位置とみなす、距離の 2 乗の下限。同じ位置の種どうしは、真ん中の線が決まらないので切らない
constexpr float kMinSeedSeparationSq = 1e-12f;
// 領域として残す多角形の、頂点の最小数
constexpr size_t kMinPolygonVertices = 3;

float Dot(const Vector2 &a, const Vector2 &b) { return a.x * b.x + a.y * b.y; }
Vector2 Sub(const Vector2 &a, const Vector2 &b) { return {a.x - b.x, a.y - b.y}; }

bool IsInside(const FractureRect &rect, const Vector2 &p) {
  return p.x >= rect.min.x && p.x <= rect.max.x && p.y >= rect.min.y && p.y <= rect.max.y;
}

// 凸多角形を、半平面で切り、半平面の中に残る部分を返す(Sutherland-Hodgman 法)。
// 半平面: signedDistance(p) = Dot(p - mid, normal) が 0 以下の側(normal の逆向き側)
// polygon: 切られる凸多角形。mid: 切る線上の 1 点。normal: 切る線の法線(長さは何でもよい)
std::vector<Vector2> ClipByHalfPlane(const std::vector<Vector2> &polygon, const Vector2 &mid, const Vector2 &normal) {
  std::vector<Vector2> result;
  const size_t count = polygon.size();
  result.reserve(count + 1);

  for (size_t i = 0; i < count; ++i) {
    const Vector2 &a = polygon[i];
    const Vector2 &b = polygon[(i + 1) % count];
    const float da = Dot(Sub(a, mid), normal);
    const float db = Dot(Sub(b, mid), normal);
    const bool aInside = da <= kPlaneEpsilon;
    const bool bInside = db <= kPlaneEpsilon;

    if (aInside) {
      result.push_back(a);
    }
    // 辺が線をまたぐときだけ、交点を足す(a が内側で b が外側、または逆)
    if (aInside != bInside) {
      const float t = da / (da - db); // 辺 a -> b の上で、符号付き距離が 0 になる位置
      result.push_back({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
    }
  }
  return result;
}
} // namespace

std::vector<Vector2> VoronoiFracture::GenerateSeeds(const FractureRect &rect, const Vector2 &impact, const FractureSettings &settings) {
  std::vector<Vector2> seeds;
  if (settings.seedCount <= 0) {
    return seeds;
  }
  seeds.reserve(static_cast<size_t>(settings.seedCount));

  // 衝撃点から、rect の一番遠い角までの距離。種を置く半径の上限にする
  const float farX = (std::max)(std::abs(rect.min.x - impact.x), std::abs(rect.max.x - impact.x));
  const float farY = (std::max)(std::abs(rect.min.y - impact.y), std::abs(rect.max.y - impact.y));
  const float maxRadius = std::sqrt(farX * farX + farY * farY);

  const float minDistanceSq = settings.minSeedDistance * settings.minSeedDistance;
  std::mt19937 random(settings.randomSeed);
  std::uniform_real_distribution<float> uniform01(0.0f, 1.0f);

  const int maxAttempts = settings.seedCount * kMaxAttemptsPerSeed;
  for (int attempt = 0; attempt < maxAttempts && static_cast<int>(seeds.size()) < settings.seedCount; ++attempt) {
    // 衝撃点を中心に、半径と角度で点を引く。半径は u の concentration 乗にするので、u が 0 〜 1 の一様でも、
    // concentration が 1 より大きいほど、小さい半径(衝撃点の近く)が出やすくなる
    const float radius = maxRadius * std::pow(uniform01(random), settings.concentration);
    const float angle = kTwoPi * uniform01(random);
    const Vector2 p{impact.x + radius * std::cos(angle), impact.y + radius * std::sin(angle)};

    if (!IsInside(rect, p)) {
      continue;
    }
    const bool tooClose = std::any_of(seeds.begin(), seeds.end(), [&](const Vector2 &s) {
      const Vector2 d = Sub(s, p);
      return Dot(d, d) < minDistanceSq;
    });
    if (!tooClose) {
      seeds.push_back(p);
    }
  }
  return seeds;
}

std::vector<VoronoiCell> VoronoiFracture::Build(const FractureRect &rect, const std::vector<Vector2> &seeds) {
  std::vector<VoronoiCell> cells;
  cells.reserve(seeds.size());

  // 反時計回りの長方形。ここから、種ごとに切り落としていく
  const std::vector<Vector2> rectPolygon = {{rect.min.x, rect.min.y}, {rect.max.x, rect.min.y}, {rect.max.x, rect.max.y}, {rect.min.x, rect.max.y}};

  for (size_t i = 0; i < seeds.size(); ++i) {
    std::vector<Vector2> polygon = rectPolygon;

    for (size_t j = 0; j < seeds.size() && !polygon.empty(); ++j) {
      if (i == j) {
        continue;
      }
      const Vector2 toOther = Sub(seeds[j], seeds[i]);
      if (Dot(toOther, toOther) < kMinSeedSeparationSq) {
        continue;
      }
      // 種 i と種 j の真ん中の点。この点を通り、toOther に垂直な線が、2 つの領域の境目になる。
      // 種 j 側(toOther の向き)を切り落とし、種 i 側を残す
      const Vector2 mid{(seeds[i].x + seeds[j].x) * 0.5f, (seeds[i].y + seeds[j].y) * 0.5f};
      polygon = ClipByHalfPlane(polygon, mid, toOther);
    }

    if (polygon.size() >= kMinPolygonVertices) {
      VoronoiCell cell;
      cell.seed = seeds[i];
      cell.polygon = std::move(polygon);
      cells.push_back(std::move(cell));
    }
  }
  return cells;
}

float VoronoiFracture::PolygonArea(const std::vector<Vector2> &polygon) {
  // 靴ひも公式: 隣り合う頂点の外積の合計の半分
  float twiceArea = 0.0f;
  for (size_t i = 0; i < polygon.size(); ++i) {
    const Vector2 &a = polygon[i];
    const Vector2 &b = polygon[(i + 1) % polygon.size()];
    twiceArea += a.x * b.y - b.x * a.y;
  }
  return twiceArea * 0.5f;
}
