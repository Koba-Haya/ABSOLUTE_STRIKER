#include "ItemEffectApplier.h"

#include <algorithm>
#include <cmath>

#include "../actor/Player/LockOnSystem.h"
#include "../actor/Player/PlayerComponent.h"
#include "PlayerUpgradeState.h"

bool ItemEffectApplier::CanApply(const ItemDefinition &item, const PlayerUpgradeState &state, const ItemUpgradeSettings &settings) {
  return state.GetRemainingEarnable(item.effect, settings.Get(item.effect).maxStacks) > 0;
}

bool ItemEffectApplier::Apply(const ItemDefinition &item, PlayerUpgradeState &state, const ItemUpgradeSettings &settings,
                              PlayerComponent &player) {
  const int added = state.AddEarned(item.effect, item.amount, settings.Get(item.effect).maxStacks);
  SyncToPlayer(state, settings, player);
  return added > 0;
}

float ItemEffectApplier::GetValue(ItemEffectKind kind, const PlayerUpgradeState &state, const ItemUpgradeSettings &settings) {
  const ItemEffectSpec &spec = settings.Get(kind);
  // 合計(継承した分 + 今回取った分)は、上限で頭打ちにする(継承の倍率で、上限を超えないように)
  const float stacks = (std::min)(state.GetTotal(kind), static_cast<float>(spec.maxStacks));
  return stacks * spec.perStack;
}

void ItemEffectApplier::SyncToPlayer(const PlayerUpgradeState &state, const ItemUpgradeSettings &settings, PlayerComponent &player) {
  auto value = [&](ItemEffectKind kind) { return GetValue(kind, state, settings); };

  player.SetMaxLockTargets(LockOnSystem::kDefaultMaxLockTargets + static_cast<int>(std::lround(value(ItemEffectKind::LockOnMax))));
  player.SetMaxHp(PlayerComponent::kBaseMaxHp + static_cast<int>(std::lround(value(ItemEffectKind::MaxHp))));
  player.SetLockRadius(LockOnSystem::kDefaultLockRadiusPixels + value(ItemEffectKind::LockRadius));
  player.SetDodgeCooldownReduction(value(ItemEffectKind::DodgeCooldown));
  player.SetFireIntervalScale(1.0f - value(ItemEffectKind::FireRate));
}
