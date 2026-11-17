#include "PlayerUpgradeState.h"

#include <algorithm>

PlayerUpgradeState &PlayerUpgradeState::Persistent() {
  static PlayerUpgradeState instance;
  return instance;
}

void PlayerUpgradeState::BeginStage(bool carryOver) {
  if (!carryOver) {
    stacks_.fill(0);
  }
}

int PlayerUpgradeState::AddStacks(ItemEffectKind kind, int amount, int maxStacks) {
  int &stacks = stacks_[static_cast<size_t>(kind)];
  const int before = stacks;
  stacks = std::clamp(stacks + amount, 0, std::max(0, maxStacks));
  return stacks - before;
}
