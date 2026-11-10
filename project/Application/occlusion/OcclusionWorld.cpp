#include "OcclusionWorld.h"
#include <algorithm>

OcclusionWorld &OcclusionWorld::GetInstance() {
  static OcclusionWorld instance;
  return instance;
}

void OcclusionWorld::Register(const IOccluderSet *set) {
  if (!set) return;
  if (std::find(sets_.begin(), sets_.end(), set) != sets_.end()) return; // 二重登録は無視
  sets_.push_back(set);
}

void OcclusionWorld::Unregister(const IOccluderSet *set) {
  sets_.erase(std::remove(sets_.begin(), sets_.end(), set), sets_.end());
}

void OcclusionWorld::Clear() { sets_.clear(); }

bool OcclusionWorld::IsSegmentBlocked(const Vector3 &from, const Vector3 &to, uint32_t kinds) const {
  for (const IOccluderSet *set : sets_) {
    // 問い合わせ側が選んだ種類だけを調べる(島は遮るが、雲は遮らない、などを呼び出しごとに選べる)
    if ((set->GetKind() & kinds) == 0) continue;
    if (set->IsSegmentBlocked(from, to)) return true;
  }
  return false;
}
