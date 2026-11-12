#include "PlayerUpgradeState.h"

#include <algorithm>

PlayerUpgradeState &PlayerUpgradeState::Persistent() {
  static PlayerUpgradeState instance;
  return instance;
}

void PlayerUpgradeState::BeginStage(bool carryOver) {
  if (!carryOver) {
    lockOnBonus_ = 0;
  }
}

int PlayerUpgradeState::AddLockOnBonus(int amount, int maxBonus) {
  const int before = lockOnBonus_;
  lockOnBonus_ = std::clamp(lockOnBonus_ + amount, 0, std::max(0, maxBonus));
  return lockOnBonus_ - before;
}
