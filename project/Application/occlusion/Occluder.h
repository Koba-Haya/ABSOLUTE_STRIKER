#pragma once
#include "Type/Vector.h"
#include <cstdint>

// 遮蔽物(視線や弾を遮るもの)の種類。ビット旗として扱い、問い合わせ側が「どの種類を遮蔽として扱うか」を選ぶ。
// 種類ごとに、遮られたときの意味が違うため(島: 見えない、雲: 見えるが狙えない)、問い合わせのたびに選べるようにしている。
// 新しい種類(雲など)を足すときは、ここに旗を足して、IOccluderSet を実装したクラスを OcclusionWorld へ登録する
namespace OccluderKind {
constexpr uint32_t kIsland = 1u << 0; // 浮島。完全に隠す(見えない、ロックオンできない、弾が当たらない、撃たない)
constexpr uint32_t kCloud = 1u << 1;  // 雲(まだ無い。将来の予約。見えるがロックオンできない、撃ってよい、通常弾は当たる)
constexpr uint32_t kAll = 0xFFFFFFFFu; // すべての種類
} // namespace OccluderKind

// 同じ種類の遮蔽物のまとまり。「2 点を結ぶ線分が、このまとまりのどれかに遮られるか」だけを答える。
// 責務を問い合わせ 1 つに絞ることで、島(円柱)と雲(別の形)のように、形も持ち方も違う遮蔽物を、同じ窓口で扱える
class IOccluderSet {
public:
  virtual ~IOccluderSet() = default;

  // このまとまりの種類(OccluderKind の旗 1 つ)
  virtual uint32_t GetKind() const = 0;

  // 点 from から点 to までの線分が、このまとまりのどれかに遮られるなら true。
  // 端点が遮蔽物の内側にある場合も、遮られるとみなす。毎フレーム、弾の数だけ呼ばれるので、軽く保つこと
  virtual bool IsSegmentBlocked(const Vector3 &from, const Vector3 &to) const = 0;
};
