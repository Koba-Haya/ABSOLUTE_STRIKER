#pragma once
#include "../hud/UpgradeToast.h"
#include "ItemDropTable.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {
class GameObject;
class ISceneQuery;
class ISpawner;
}
struct ItemDefinition;
class ItemCatalog;
class InheritanceState;
class PlayerUpgradeState;
enum class ItemEffectKind;

// アイテムの仕組み全体の窓口。GameScene は、このクラスの関数だけを呼ぶ。
//   BeginStage: ステージの開始(自機が決まった後)に 1 回
//   OnEnemyDestroyed: 敵が倒れたときに 1 回
//   Update / Draw: 毎フレーム(強化を取ったことを知らせる文字の、動きと描画)
// 分けた理由: シーンに処理を書かず、各クラス(抽選、出現、取得、効果、強化の状態)をつなぐ役を 1 か所にまとめるため。
//             落ちているアイテムの動き、取得判定は、各アイテムのコンポーネント(ItemPickupComponent)が行うので、毎フレームの更新は要らない。
class ItemSystem {
public:
  // catalog: アイテムの定義と設定。inheritance: 前の世代から引き継いだ強化(読むだけ)。upgrades: 自機の強化の状態(取ると増える)。
  // どれも所有しない(GameData が持つ。このクラスより長く生きること)
  ItemSystem(const ItemCatalog &catalog, const InheritanceState &inheritance, PlayerUpgradeState &upgrades);

  // ステージの開始で呼ぶ。強化の状態を(設定に従って)初期化し、持ち越した強化を自機へ反映する
  void BeginStage(const std::shared_ptr<AbsoluteEngine::GameObject> &player);

  // 自機のオブジェクトが作り直されたとき(エディタの Stop による復元など)に呼ぶ。
  // 新しい自機を覚え直し、いまの強化の状態(PlayerUpgradeState)を、新しい自機へ反映する。強化の状態そのもの(ステージの開始の初期化)は、変えない
  void RebindPlayer(const std::shared_ptr<AbsoluteEngine::GameObject> &player);

  // 毎フレーム: 取ったことを知らせる文字を動かす(deltaTime は実時間の 1 フレーム)
  void Update(float deltaTime);

  // 取ったことを知らせる文字を描く(HUD と同じ 2D の描画の最中に呼ぶ)
  void Draw();

  // 開発用: そのアイテムを、取ったことにする(落とさずに、効果と知らせだけを出す)。強化の効き目を、すぐ確かめるために使う
  void DebugGrant(const ItemDefinition &item) { OnCollected_(item); }

  // 展示用: 強化 kind を count 個、取ったことにする(知らせの文字は出さず、効果を自機へ反映する)。上限を超える分は、取れない。
  // BeginStage のあとに呼ぶ(ステージの開始で、今回取った分は 0 に戻るため)
  void GrantEarned(ItemEffectKind kind, int count);

  // 敵 enemyName(オブジェクトの名前 = プレハブ名)が position で倒れた。抽選して、当たればアイテムを置く
  void OnEnemyDestroyed(AbsoluteEngine::ISpawner &spawner, AbsoluteEngine::ISceneQuery &query, const std::string &enemyName, const Vector3 &position);

private:
  // 自機がアイテムを取った瞬間(効果を出す)
  void OnCollected_(const ItemDefinition &item);

  const ItemCatalog &catalog_;          // アイテムの定義と設定(所有しない)
  const InheritanceState &inheritance_; // 前の世代から引き継いだ強化(所有しない)
  PlayerUpgradeState &upgrades_;        // 自機の強化の状態(所有しない)
  ItemDropTable dropTable_;
  UpgradeToast toast_; // 強化を取ったことを知らせる文字
  bool toastReady_ = false;
  std::weak_ptr<AbsoluteEngine::GameObject> player_;
};
