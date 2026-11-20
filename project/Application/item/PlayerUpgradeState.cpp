#include "PlayerUpgradeState.h"

#include <algorithm>
#include <cmath>

namespace {
// 上限と継承した分の差を、整数へ切り捨てるときの、浮動小数点の誤差の余裕
constexpr float kFloorEpsilon = 1e-4f;
} // namespace

PlayerUpgradeState &PlayerUpgradeState::Persistent() {
  static PlayerUpgradeState instance;
  return instance;
}

void PlayerUpgradeState::BeginStage(bool carryOver) {
  if (!carryOver) {
    earned_.fill(0);
  }
}

int PlayerUpgradeState::GetRemainingEarnable(ItemEffectKind kind, int maxStacks) const {
  const float room = static_cast<float>(maxStacks) - GetInherited(kind);
  const int allowedEarned = (std::max)(0, static_cast<int>(std::floor(room + kFloorEpsilon)));
  return (std::max)(0, allowedEarned - GetEarned(kind));
}

int PlayerUpgradeState::AddEarned(ItemEffectKind kind, int amount, int maxStacks) {
  const int added = (std::min)(amount, GetRemainingEarnable(kind, maxStacks));
  if (added <= 0) return 0;
  earned_[static_cast<size_t>(kind)] += added;
  return added;
}
