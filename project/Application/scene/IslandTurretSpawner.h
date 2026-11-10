#pragma once
#include <vector>

class BaseScene;
class StageIsland;
class StageIslands;

// 島のタレット(既存の EnemyTurret のプレハブ)を、タレットの島の上面へ出すクラス。
// 責務は「自機が近づいたら、島の上へタレットを 1 つ出す」だけ。島の見た目と配置は StageIslands、タレットの動き(撃つ、撃たれる)は
// プレハブの部品(TurretShootComponent、EnemyComponent など)が持つ。
// クラスを分けた理由: 島の描画と、敵の生成(シーンへの追加)は別の持ち物で、依存を一方向(ここが StageIslands を読む)に保つため。
//
// 出し方の理由: タイムライン(時間 + ワールド座標)ではなく、レール上の距離で出す。島の位置は配置の計算で決まるので、
// タイムラインの JSON に位置を写すと、島の配置を変えたときにずれる。また、レール上の距離なので、ブーストや遅れで自機の進みが変わっても、
// 島に近づいた時点で出る。出す距離(kSpawnAhead)は、島が見え始める距離(StageIslandDefaults::kDrawAhead)より近く、
// 敵が撃ち始める距離として遠すぎない値にしている(理由は Update のコメント)。
// 敵の生成は、BaseScene::AddRootObject を通す。GameScene がそれを上書きして、撃破時のスコアなどのコールバックを付ける。
class IslandTurretSpawner {
public:
  // 毎フレーム呼ぶ(プレイ中)。railDistance: 自機(カメラ)のいる、レール上の距離。
  // 自機がタレットの島へ kSpawnAhead まで近づいたら、タレットを出す。出したあと、倒されても、もう一度は出さない。
  // レールの進みが、出す距離より手前へ戻ったとき(デバッグのやり直しなど)は、出し直せる状態に戻す
  void Update(BaseScene &scene, const StageIslands &islands, float railDistance);

private:
  // 島ごとの状態(出したか)。倒されたタレットを、もう一度出さないための印
  struct SlotState {
    bool spawned = false;
  };
  // 島の上面の頂点に乗せて、タレットを 1 つ出す。出せたら true
  bool SpawnTurret_(BaseScene &scene, const StageIsland &island) const;

  std::vector<SlotState> states_;
};
