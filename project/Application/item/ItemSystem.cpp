#include "ItemSystem.h"

#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Player/PlayerComponent.h"
#include "ItemCatalog.h"
#include "ItemEffectApplier.h"
#include "ItemSpawner.h"
#include "PlayerUpgradeState.h"

void ItemSystem::BeginStage(const std::shared_ptr<AbsoluteEngine::GameObject> &player) {
  player_ = player;
  const ItemUpgradeSettings &settings = ItemCatalog::GetInstance().GetUpgradeSettings();
  PlayerUpgradeState &state = PlayerUpgradeState::Persistent();
  // 持ち越さない設定なら 0 に戻る。持ち越す設定なら、前のステージの値を、ここで自機へ戻す
  state.BeginStage(settings.carryOverBetweenStages);
  if (player) {
    if (auto *playerComp = player->GetComponent<PlayerComponent>()) {
      ItemEffectApplier::SyncToPlayer(state, settings, *playerComp);
    }
  }
}

void ItemSystem::OnEnemyDestroyed(BaseScene &scene, const std::string &enemyName, const Vector3 &position) {
  const ItemCatalog &catalog = ItemCatalog::GetInstance();
  const PlayerUpgradeState &state = PlayerUpgradeState::Persistent();

  // 今取る意味があるアイテムだけを、抽選の対象にする(上限に達した強化は出さない)
  const ItemDefinition *item = dropTable_.Roll(enemyName, [&](const ItemDefinition &candidate) {
    return ItemEffectApplier::CanApply(candidate, state, catalog.GetUpgradeSettings());
  });
  if (!item) return;

  ItemSpawner::Spawn(scene, *item, catalog.GetPickupSettings(), position, [this, item]() { OnCollected_(*item); });
}

void ItemSystem::OnCollected_(const ItemDefinition &item) {
  const auto player = player_.lock();
  if (!player) return;
  auto *playerComp = player->GetComponent<PlayerComponent>();
  if (!playerComp) return;

  ItemEffectApplier::Apply(item, PlayerUpgradeState::Persistent(), ItemCatalog::GetInstance().GetUpgradeSettings(), *playerComp);
}
