#pragma once
#include "CollisionShape.h"

namespace AbsoluteEngine {

// 形状の組ごとの衝突の計算(Double Dispatch で振り分けた先の、実際の計算)。
// 球と箱の計算は、球の側からも箱の側からも使うので、どちらのクラスにも属さない関数として 1 か所に置く。
// 結果の法線は、どれも「先に指定した側」を押し出す向き
namespace ShapePairTests {

// 球 a と球 b
CollisionResult SphereSphere(const SphereShape& a, const ShapePlacement& pa, const SphereShape& b, const ShapePlacement& pb);
// 箱 a と箱 b
CollisionResult AabbAabb(const AabbShape& a, const ShapePlacement& pa, const AabbShape& b, const ShapePlacement& pb);
// 球と箱(法線は、球を押し出す向き)
CollisionResult SphereAabb(const SphereShape& sphere, const ShapePlacement& ps, const AabbShape& box, const ShapePlacement& pb);

// 結果の法線を反対向きにする(「先に指定した側」を入れ替えたときに使う)
CollisionResult Flip(CollisionResult result);

} // namespace ShapePairTests

} // namespace AbsoluteEngine
