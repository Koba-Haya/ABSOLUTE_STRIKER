#pragma once
#include <memory>

#include "../actor/Enemy/IEnemyDefeatObserver.h"

namespace AbsoluteEngine {
class GameObject;
class ISpawner;
class ISceneQuery;
}
class HitEffectSpawner;
class ItemSystem;
class ScoreTracker;
class RunStatsTracker;

// 敵が倒れたときの「報酬の係」(Observer パターンの Observer)。
// 倒れた敵から知らせを受け、爆発の演出、ドロップアイテム、スコア、評価の計測、編隊の全滅の判定を行う。
//
// 分けた理由: 以前は、GameScene が敵ごとにラムダを登録し、その中に全部の処理を書いていた(シーンに処理が直接入っていた)。
// 「敵を倒したら何が起きるか」を 1 つのクラスにまとめ、シーンは、敵が出たときに Track を呼ぶだけにした。
// 敵(EnemyComponent)は、このクラスを知らない(基底 IEnemyDefeatObserver だけを知る)。
//
// 寿命: 敵はこのクラスをポインタで持つ(所有しない)。持ち主の GameScene のメンバとして、敵より長く生きる。
// シーンの終わりは、オブジェクトの一覧より先に消えるが、消える途中の敵は知らせを出さないので、安全。
class EnemyDefeatRewards final : public IEnemyDefeatObserver {
public:
  // 使う係と窓口を受け取る。いずれも所有しない。このクラスより長く生きること
  EnemyDefeatRewards(HitEffectSpawner &hitEffects, ItemSystem &items, ScoreTracker &score, RunStatsTracker &runStats,
                     AbsoluteEngine::ISpawner &spawner, AbsoluteEngine::ISceneQuery &query);

  // obj が敵(EnemyComponent を持つ)なら、倒れたことの受け取りを登録し、出現を数える(編隊の数、評価の母数)。
  // 敵でなければ何もしない。同じ敵に 2 回呼ばれても、登録と数えは 1 回だけ
  void Track(const std::shared_ptr<AbsoluteEngine::GameObject> &obj);

  // IEnemyDefeatObserver: 倒れた敵 1 体につき 1 回呼ばれる
  void OnEnemyDefeated(const EnemyDefeatEvent &event) override;

private:
  HitEffectSpawner &hitEffects_;      // 倒れた位置の爆発(粒、輪、光)
  ItemSystem &items_;                 // ドロップアイテム
  ScoreTracker &score_;               // スコア、撃破数、編隊の残数
  RunStatsTracker &runStats_;         // リザルトの評価の元になる数字
  AbsoluteEngine::ISpawner &spawner_; // 爆発の光やアイテムを出す窓口
  AbsoluteEngine::ISceneQuery &query_; // アイテムが自機を探す窓口
};
