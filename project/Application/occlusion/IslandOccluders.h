#pragma once
#include "Application/occlusion/Occluder.h"
#include "Type/Vector.h"
#include <cstddef>
#include <vector>

// 島のモデルの種類(遮蔽の形を選ぶためだけの区別)。StageIslands の GateIslandKind と、並びも意味も同じ
enum class IslandModelKind {
  Small,
  Medium,
  Forest,
  Rock,
};

// 島の遮蔽の形に使う、単純な形(垂直な円柱)の寸法。島のモデルの座標(大きさの倍率 1 のとき)で書く。
// 値は、各モデルの頂点データを高さで 6 段に分けて、段ごとの最大の半径を調べた結果から決めた:
//   forest 最大 3.47 / medium 2.83 / small 1.69 / rock 1.71(下へ行くほど細い)
// 半径は、最大の約 8 割にする。島の見た目より少し小さい円柱にして、「島の縁のそばを通るのに、見えない壁に当たる」誤りを避ける
// (逆に、島の縁の少しの隙間から、隠れた敵が見えることは、許す)。
// 上面の高さ(top)は、頂点や木より下にする。タレットは島の頂点の上に乗るので、円柱がタレットを隠さないようにするため。
// 下面の高さ(bottom)は、島が細くなる手前で切る(下は雲に埋まるので、細い部分の遮蔽は要らない)
struct IslandOccluderShape {
  float radius; // 円柱の半径
  float bottom; // 円柱の下面の高さ(島の原点から。倍率 1)
  float top;    // 円柱の上面の高さ(島の原点から。倍率 1)
};
constexpr IslandOccluderShape kIslandOccluderShapes[] = {
    {1.4f, -2.2f, 0.5f}, // Small
    {2.3f, -3.5f, 0.4f}, // Medium
    {2.8f, -3.4f, 0.3f}, // Forest
    {1.4f, -3.0f, 0.2f}, // Rock
};

// 浮島の遮蔽物のまとまり。島 1 つを、垂直な円柱で近似して持つ。
// 責務は「島の位置と大きさを、円柱として持つこと」と「線分が円柱に遮られるかの判定」だけ。島の配置や描画は StageIslands の仕事。
// 円柱で近似する理由: 島は横に広く(半径は高さの半分ほど)、向きや傾きが様々。垂直な円柱なら、向き(yaw)で形が変わらず、
// 判定が「高さの範囲で線分を切る + 平面上で円と線分の距離」だけで済む。傾き(最大約 6 度)は無視する
class IslandOccluders : public IOccluderSet {
public:
  // 島を 1 つ足す。position は島の原点(上面の中心。モデルの原点)、scale は一様な拡大
  void AddIsland(IslandModelKind kind, const Vector3 &position, float scale);
  // 島をすべて消す(作り直しの前に呼ぶ)
  void Clear() { cylinders_.clear(); }
  size_t GetCount() const { return cylinders_.size(); }

  uint32_t GetKind() const override { return OccluderKind::kIsland; }
  bool IsSegmentBlocked(const Vector3 &from, const Vector3 &to) const override;

private:
  // 円柱 1 つ。ワールド座標で持つ(問い合わせのたびに倍率を掛けなくて済むように、足すときに計算しておく)
  struct Cylinder {
    float centerX;
    float centerZ;
    float radius;
    float bottom; // 下面の高さ(ワールド)
    float top;    // 上面の高さ(ワールド)
  };
  std::vector<Cylinder> cylinders_;
};
