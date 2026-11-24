#include "ItemSystem.h"

#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Player/PlayerComponent.h"
#include "../inheritance/InheritanceState.h"
#include "ItemCatalog.h"
#include "ItemEffectApplier.h"
#include "ItemSpawner.h"
#include "PlayerUpgradeState.h"

ItemSystem::ItemSystem(const ItemCatalog &catalog, const InheritanceState &inheritance)
    : catalog_(catalog), inheritance_(inheritance), dropTable_(catalog) {}

void ItemSystem::BeginStage(const std::shared_ptr<AbsoluteEngine::GameObject> &player) {
  player_ = player;
  if (!toastReady_) {
    toast_.Initialize(); // 文字の資源の読み込みは、最初の 1 回だけ
    toastReady_ = true;
  }
  const ItemUpgradeSettings &settings = catalog_.GetUpgradeSettings();
  PlayerUpgradeState &state = PlayerUpgradeState::Persistent();
  // 持ち越さない設定なら 0 に戻る。持ち越す設定なら、前のステージの値を、ここで自機へ戻す
  state.BeginStage(settings.carryOverBetweenStages);
  // 継承した分(前の世代から引き継いだ強化)を、ここで入れる。ゲームオーバーからのやり直しでも、同じ値が入る
  for (size_t i = 0; i < kItemEffectKindCount; ++i) {
    const ItemEffectKind kind = static_cast<ItemEffectKind>(i);
    state.SetInherited(kind, inheritance_.GetInheritedStacks(kind));
  }
  RebindPlayer(player);
}

void ItemSystem::RebindPlayer(const std::shared_ptr<AbsoluteEngine::GameObject> &player) {
  player_ = player;
  if (!player) return;
  if (auto *playerComp = player->GetComponent<PlayerComponent>()) {
    ItemEffectApplier::SyncToPlayer(PlayerUpgradeState::Persistent(), catalog_.GetUpgradeSettings(), *playerComp);
  }
}

void ItemSystem::GrantEarned(ItemEffectKind kind, int count) {
  const ItemUpgradeSettings &settings = catalog_.GetUpgradeSettings();
  PlayerUpgradeState &state = PlayerUpgradeState::Persistent();
  state.AddEarned(kind, count, settings.Get(kind).maxStacks);
  // 自機へ反映する(最大 HP などの効果が、すぐ出る)
  if (const auto player = player_.lock()) {
    if (auto *playerComp = player->GetComponent<PlayerComponent>()) {
      ItemEffectApplier::SyncToPlayer(state, settings, *playerComp);
    }
  }
}

void ItemSystem::OnEnemyDestroyed(AbsoluteEngine::ISpawner &spawner, AbsoluteEngine::ISceneQuery &query, const std::string &enemyName, const Vector3 &position) {
  const ItemCatalog &catalog = catalog_;
  const PlayerUpgradeState &state = PlayerUpgradeState::Persistent();

  // 今取る意味があるアイテムだけを、抽選の対象にする(上限に達した強化は出さない)
  const ItemDefinition *item = dropTable_.Roll(enemyName, [&](const ItemDefinition &candidate) {
    return ItemEffectApplier::CanApply(candidate, state, catalog.GetUpgradeSettings());
  });
  if (!item) return;

  ItemSpawner::Spawn(spawner, query, *item, catalog.GetPickupSettings(), catalog.GetUpgradeSettings(), position, [this, item]() { OnCollected_(*item); });
}

void ItemSystem::OnCollected_(const ItemDefinition &item) {
  const auto player = player_.lock();
  if (!player) return;
  auto *playerComp = player->GetComponent<PlayerComponent>();
  if (!playerComp) return;

  // 効果が実際に出たときだけ、取ったことを知らせる(上限で何も起きなかったときは、出さない)
  if (ItemEffectApplier::Apply(item, PlayerUpgradeState::Persistent(), catalog_.GetUpgradeSettings(), *playerComp)) {
    toast_.Show(item.name);
  }
}

void ItemSystem::Update(float deltaTime) {
  toast_.Update(deltaTime);
}

void ItemSystem::Draw() {
  toast_.Draw();
}
