#include "EnemyDefeatRewards.h"

#include "AbsoluteEngine/scene/GameObject.h"
#include "HitEffectSpawner.h"
#include "ScoreTracker.h"
#include "../actor/Enemy/EnemyComponent.h"
#include "../actor/Enemy/FormationMemberComponent.h"
#include "../item/ItemSystem.h"
#include "../result/RunStatsTracker.h"

EnemyDefeatRewards::EnemyDefeatRewards(HitEffectSpawner &hitEffects, ItemSystem &items, ScoreTracker &score, RunStatsTracker &runStats,
                                       AbsoluteEngine::ISpawner &spawner, AbsoluteEngine::ISceneQuery &query)
    : hitEffects_(hitEffects), items_(items), score_(score), runStats_(runStats), spawner_(spawner), query_(query) {}

void EnemyDefeatRewards::Track(const std::shared_ptr<AbsoluteEngine::GameObject> &obj) {
  if (!obj) return;
  auto *enemy = obj->GetComponent<EnemyComponent>();
  if (!enemy) return;

  // 登録済み(同じ敵に 2 回呼ばれた)なら、出現を 2 回数えないよう、ここで終える
  if (!enemy->AddObserver(this)) return;

  // 編隊のメンバーなら、編隊の初期の人数を積んでおく(全滅の判定に使う)
  const auto *formation = obj->GetComponent<FormationMemberComponent>();
  if (formation) {
    score_.RegisterFormationMember(formation->GetFormationId());
  }
  // リザルトの評価のための計測: 敵の出現(編隊のメンバーなら、編隊の ID も)
  runStats_.OnEnemySpawned(formation != nullptr, formation ? formation->GetFormationId() : 0);
}

void EnemyDefeatRewards::OnEnemyDefeated(const EnemyDefeatEvent &event) {
  // 爆発(粒、輪、光)。倒れた理由に関わらず出す
  hitEffects_.Spawn(spawner_, event.position);

  // 自滅(体当たりなど)は、自機が倒したのではないので、以降の報酬(アイテム、スコア、撃破数、編隊の判定)は出さない。
  // 編隊の判定を数えないので、自滅した者がいる編隊は、全滅ボーナスが出ない(全員を自分で倒したときだけ出る)
  if (event.reason != EnemyDestroyReason::Player) {
    runStats_.OnEnemySelfDestructed(); // 自滅した敵は、撃破率の母数から外す(評価のための計測)
    return;
  }

  // ドロップアイテム(抽選に当たれば、倒れた位置に置く)。敵の種類は、オブジェクトの名前(= プレハブ名)で見分ける
  items_.OnEnemyDestroyed(spawner_, query_, event.enemy.GetName(), event.position);

  // スコア加算
  score_.OnEnemyKilled();
  runStats_.OnEnemyKilled();

  // 編隊の判定: 編隊が全滅したら全滅ボーナスを加算する。戻り値(初期メンバー数)が 0 より大きいときが、全滅の合図
  if (const auto *formation = event.enemy.GetComponent<FormationMemberComponent>()) {
    if (score_.OnFormationMemberKilled(formation->GetFormationId()) > 0) {
      runStats_.OnFormationCleared();
    }
  }
}
