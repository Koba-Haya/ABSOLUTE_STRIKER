#include "ItemEffectApplier.h"

#include <algorithm>

#include "../actor/Player/LockOnSystem.h"
#include "../actor/Player/PlayerComponent.h"
#include "PlayerUpgradeState.h"

namespace {
// ロックオン上限の加算分の最大(上限の最大値 - 初期値)。設定の最大値が初期値より小さいときは 0
int MaxLockOnBonus(const ItemUpgradeSettings &settings) {
  return std::max(0, settings.lockOnMaxCap - LockOnSystem::kDefaultMaxLockTargets);
}
} // namespace

bool ItemEffectApplier::CanApply(const ItemDefinition &item, const PlayerUpgradeState &state, const ItemUpgradeSettings &settings) {
  switch (item.effect) {
  case ItemEffectKind::LockOnMax:
    return state.GetLockOnBonus() < MaxLockOnBonus(settings);
  }
  return false;
}

bool ItemEffectApplier::Apply(const ItemDefinition &item, PlayerUpgradeState &state, const ItemUpgradeSettings &settings,
                              PlayerComponent &player) {
  switch (item.effect) {
  case ItemEffectKind::LockOnMax: {
    const int added = state.AddLockOnBonus(item.amount, MaxLockOnBonus(settings));
    SyncToPlayer(state, settings, player);
    return added > 0;
  }
  }
  return false;
}

void ItemEffectApplier::SyncToPlayer(const PlayerUpgradeState &state, const ItemUpgradeSettings & /*settings*/, PlayerComponent &player) {
  player.SetMaxLockTargets(LockOnSystem::kDefaultMaxLockTargets + state.GetLockOnBonus());
}
