#pragma once
#include "Vector.h"
#include <cstdint>
#include <vector>

// ボロノイ分割の、分ける対象(長方形)。板の平面上の座標で、min が左下、max が右上
struct FractureRect {
  Vector2 min;
  Vector2 max;
};

// 分割でできた 1 つの領域(破片の元になる)
struct VoronoiCell {
  Vector2 seed;                 // この領域の種の点(この領域の中のどの点も、他のどの種よりこの種に近い)
  std::vector<Vector2> polygon; // 領域の輪郭。反時計回りの凸多角形(頂点が 3 つ以上)
};

namespace FractureDefaults {
// 種の数(欲しい数。条件を満たす置き場所が足りないと、これより少なくなる)
constexpr int kSeedCount = 48;
// 衝撃点への集まり具合。1 で一様。大きいほど、衝撃点の近くに種が集まる(= 近くが細かく割れる)
constexpr float kConcentration = 2.0f;
// 種どうしの最小の距離(板の座標の単位)。近すぎる種は、細すぎる破片になるので置かない
constexpr float kMinSeedDistance = 0.04f;
// 乱数の種。同じ値なら、毎回同じ割れ方になる(デモで毎回変えたいときは、呼ぶ側で値を変える)
constexpr uint32_t kRandomSeed = 1;
} // namespace FractureDefaults

struct FractureSettings {
  int seedCount = FractureDefaults::kSeedCount;
  float concentration = FractureDefaults::kConcentration;
  float minSeedDistance = FractureDefaults::kMinSeedDistance;
  uint32_t randomSeed = FractureDefaults::kRandomSeed;
};

// 長方形を、ボロノイ分割で凸多角形の領域に分ける。
// ボロノイ分割: 平面にいくつか「種の点」を置き、平面の各点を「いちばん近い種」で塗り分ける方法。
// 塗り分けた領域は、ひび割れたガラスのような凸多角形になる。種を 1 か所に集めて置くと、そこだけ細かく割れる。
// 板の描画やゲームの状態は知らない(数学だけ)。破片のメッシュ作りや動きは、呼ぶ側が行う。
class VoronoiFracture {
public:
  // 衝撃点の近くに集まるように、種の点を置く。
  // rect: 種を置ける範囲。impact: 衝撃点(この近くほど種が多くなる)
  // 戻り値: 種の点(rect の中。settings.seedCount 以下の数)
  static std::vector<Vector2> GenerateSeeds(const FractureRect &rect, const Vector2 &impact, const FractureSettings &settings);

  // rect を、種の点で分割する。
  // 作り方: 種ごとに、まず rect 全体を領域とし、他の種との「真ん中の線(垂直二等分線)」で順に切り落としていく
  // 戻り値: 領域の一覧。領域の面積の合計は rect の面積に等しい。種が 1 つも無いときは空
  // 注意: 同じ位置の種が複数あると、それらは同じ領域を重ねて持つ(面積の合計が合わなくなる)。
  //       GenerateSeeds は近すぎる種を置かないので、通常は起きない
  static std::vector<VoronoiCell> Build(const FractureRect &rect, const std::vector<Vector2> &seeds);

  // 多角形の面積(頂点が反時計回りなら正)
  static float PolygonArea(const std::vector<Vector2> &polygon);
};
