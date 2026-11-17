#include "ItemEffectApplier.h"

#include <cmath>

#include "../actor/Player/LockOnSystem.h"
#include "../actor/Player/PlayerComponent.h"
#include "PlayerUpgradeState.h"

bool ItemEffectApplier::CanApply(const ItemDefinition &item, const PlayerUpgradeState &state, const ItemUpgradeSettings &settings) {
  return state.GetStacks(item.effect) < settings.Get(item.effect).maxStacks;
}

bool ItemEffectApplier::Apply(const ItemDefinition &item, PlayerUpgradeState &state, const ItemUpgradeSettings &settings,
                              PlayerComponent &player) {
  const int added = state.AddStacks(item.effect, item.amount, settings.Get(item.effect).maxStacks);
  SyncToPlayer(state, settings, player);
  return added > 0;
}

float ItemEffectApplier::GetValue(ItemEffectKind kind, const PlayerUpgradeState &state, const ItemUpgradeSettings &settings) {
  return static_cast<float>(state.GetStacks(kind)) * settings.Get(kind).perStack;
}

void ItemEffectApplier::SyncToPlayer(const PlayerUpgradeState &state, const ItemUpgradeSettings &settings, PlayerComponent &player) {
  auto value = [&](ItemEffectKind kind) { return GetValue(kind, state, settings); };

  player.SetMaxLockTargets(LockOnSystem::kDefaultMaxLockTargets + static_cast<int>(std::lround(value(ItemEffectKind::LockOnMax))));
  player.SetMaxHp(PlayerComponent::kBaseMaxHp + static_cast<int>(std::lround(value(ItemEffectKind::MaxHp))));
  player.SetLockRadius(LockOnSystem::kDefaultLockRadiusPixels + value(ItemEffectKind::LockRadius));
  player.SetDodgeCooldownReduction(value(ItemEffectKind::DodgeCooldown));
  player.SetFireIntervalScale(1.0f - value(ItemEffectKind::FireRate));
}
