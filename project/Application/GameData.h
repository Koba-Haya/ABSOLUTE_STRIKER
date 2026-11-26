#pragma once
#include "actor/Bullet/BulletCatalog.h"
#include "inheritance/InheritanceState.h"
#include "item/ItemCatalog.h"
#include "item/PlayerUpgradeState.h"
#include "result/RatingCatalog.h"
#include "stage/StageCatalog.h"
#include "stage/StageProgress.h"

// ゲームのデータ(読み取り専用の定義の表と、プレイで変わる保存データ)の持ち主。
//
// 分けた理由: これらは、以前は、それぞれ Singleton(GetInstance)で、どこからでも触れた。
// 「どのデータを、どの部品が読み書きするか」を、コンストラクタの引数に出して、依存を見えるようにするため、
// 持ち主を 1 つ(アプリの寿命を持つ AppServices が、このクラスを持つ)に決めた。
// シーンは、このクラスの参照を受け取り、下の部品(アイテム、UI など)へは、必要な表の参照だけを渡す。
// 読み込みは Load で明示的に行う(ファイルの場所の設定が済んだあと、アプリの初期化で 1 回呼ぶ)。
class GameData {
public:
  // 定義の表(ステージ、アイテム、評価の基準、弾)と、保存データ(進行、継承)を、ファイルから読む。
  // 定義の表は、ファイルが無い、書式が違うときは、探した場所を表示して終了する。保存データは、無い、壊れているときは、最初の状態で始める
  void Load();

  // ステージの定義の一覧
  const StageCatalog &GetStageCatalog() const { return stageCatalog_; }
  // アイテムの定義と設定
  const ItemCatalog &GetItemCatalog() const { return itemCatalog_; }
  // 評価の基準
  const RatingCatalog &GetRatingCatalog() const { return ratingCatalog_; }
  // 弾の定義(モデル、大きさ、当たり判定の半径など)
  const BulletCatalog &GetBulletCatalog() const { return bulletCatalog_; }

  // ステージの進行(解放、ベスト記録)。プレイで変わる保存データ
  StageProgress &GetStageProgress() { return stageProgress_; }
  const StageProgress &GetStageProgress() const { return stageProgress_; }
  // 継承(前の世代から引き継いだ強化と、世代数)。プレイで変わる保存データ
  InheritanceState &GetInheritanceState() { return inheritanceState_; }
  const InheritanceState &GetInheritanceState() const { return inheritanceState_; }
  // 自機の強化の状態(継承した分と、今回取った分)。ステージをまたいで残る(保存はしない)
  PlayerUpgradeState &GetPlayerUpgradeState() { return playerUpgradeState_; }
  const PlayerUpgradeState &GetPlayerUpgradeState() const { return playerUpgradeState_; }

private:
  StageCatalog stageCatalog_;
  ItemCatalog itemCatalog_;
  RatingCatalog ratingCatalog_;
  BulletCatalog bulletCatalog_;
  StageProgress stageProgress_;
  InheritanceState inheritanceState_;
  PlayerUpgradeState playerUpgradeState_;
};
