#pragma once
#include <memory>
#include <string>

#include "Type/Vector.h"
#include "externals/nlohmann/json.hpp"

namespace AbsoluteEngine {
class GameObject;
class ISpawner;
}
class BulletCatalog;
class EnemyBulletPools;
struct BulletDefinition;

// 敵の射撃部品(EnemyShoot / TurretShoot / AimedShoot)が共通で持つ、「どの弾を撃つか」と「弾を出す」役。
//
// 分けた理由: 3 つの射撃部品は、狙い方とタイミングは違うが、「プレハブの JSON の bulletId から弾の定義を引き、
// 定義どおりの弾を出す」部分は同じ。各部品に同じ処理を 3 回書かないよう、ここへまとめた(部品はこのクラスを持つ = 合成)。
// 弾は、シーンが持つ種類ごとの置き場所(EnemyBulletPools)から借りて使い回す。空きが無ければ、定義から組み立てる。
// 同じ種類の弾は、どの敵が組み立てても定義だけで同じ作りになるので、置き場所を共有しても作りが混ざらない。
//
// 速さと向きは、撃つ部品が決めて Fire へ渡す(狙い方は部品ごとに違うため)。
class EnemyBulletLauncher {
public:
  // catalog: 弾の定義の表。pools: 弾の種類ごとの置き場所(シーンが持つ)。
  // どちらも所有しない。このクラス(= 持ち主の部品)より長く生きること
  EnemyBulletLauncher(const BulletCatalog &catalog, EnemyBulletPools &pools) : catalog_(catalog), pools_(pools) {}

  // 設定中の弾を 1 発出す。position: 出す位置、velocity: 速度(ワールド空間、ユニット/秒)。
  // 戻り値: 出した弾。bulletId が未設定、または定義の表に無いときは、何も出さずに nullptr(最初の 1 回だけログに出す)
  std::shared_ptr<AbsoluteEngine::GameObject> Fire(AbsoluteEngine::ISpawner &spawner, const Vector3 &position, const Vector3 &velocity);

  // 設定中の弾の id(未設定なら空)
  const std::string &GetBulletId() const { return bulletId_; }

  // 持ち主の部品の Serialize / Deserialize から呼ぶ(JSON の "bulletId" を読み書きする)
  void Serialize(nlohmann::json &j) const;
  void Deserialize(const nlohmann::json &j);

  // 持ち主の部品の DrawInspectorUI から呼ぶ(定義の表にある弾から選ぶ)。値が変わったら true
  bool DrawInspectorUI();

private:
  // 弾の id を変える(見つからなかったときのログを、新しい id でもう一度出せるようにする)。
  // その種類の置き場所がまだ無ければ作り、定義の prewarm の数だけ弾を前もって入れる(最初の発射で、作る処理が重ならないように)
  void SetBulletId_(const std::string &bulletId);
  // 設定中の弾の定義を引く。無ければ、最初の 1 回だけ理由をログに出して nullptr
  const BulletDefinition *ResolveDefinition_();

  const BulletCatalog &catalog_;
  EnemyBulletPools &pools_;
  std::string bulletId_;          // 撃つ弾の id(プレハブの JSON の "bulletId")
  bool reportedMissing_ = false;  // 定義が無いことを、ログへ出し済みか(毎フレーム出さないため)
};
