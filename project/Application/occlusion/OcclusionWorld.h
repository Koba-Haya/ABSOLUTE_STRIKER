#pragma once
#include "Application/occlusion/Occluder.h"
#include <vector>

// 遮蔽物のまとまり(IOccluderSet)の登録所。ロックオン、弾、(段階 2 で)敵の射撃が、ここへ「この線分は遮られるか」を問い合わせる。
// クラスを分けた理由: 弾やロックオンはシーンへの参照を持たないので、遮蔽物を持つシーン(GameScene)と、問い合わせる側を、
// この窓口で切り離す。依存は、問い合わせる側 -> OcclusionWorld <- 遮蔽物を持つ側、の一方向になる。
// 登録されたまとまりの所有はしない(ポインタを借りるだけ)。持ち主が、自分が消える前に Unregister すること
class OcclusionWorld {
public:
  static OcclusionWorld &GetInstance();

  // まとまりを登録する。同じものを二重に登録しても、1 つだけ有効
  void Register(const IOccluderSet *set);
  // まとまりの登録を外す(登録されていなければ、何もしない)
  void Unregister(const IOccluderSet *set);
  // すべての登録を外す(シーンの開始時に、前のシーンの残りを消すため)
  void Clear();

  // 点 from から点 to までの線分が、kinds に含まれる種類の遮蔽物に遮られるなら true。
  // kinds は OccluderKind の旗の組み合わせ(例: 島だけ = kIsland、すべて = kAll)。登録が無ければ、常に false
  bool IsSegmentBlocked(const Vector3 &from, const Vector3 &to, uint32_t kinds = OccluderKind::kAll) const;

private:
  OcclusionWorld() = default;
  std::vector<const IOccluderSet *> sets_;
};
