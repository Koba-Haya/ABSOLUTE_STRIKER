#pragma once
#include <memory>
#include <string>
#include <unordered_map>

namespace AbsoluteEngine {
class GameObjectPool;
}

// 敵の弾の置き場所(GameObjectPool)を、弾の種類(bullets.json の id)ごとに 1 つずつ持つ集まり。
//
// 分けた理由: 敵は次々に出ては倒されるので、敵 1 体ごとに置き場所を持つと、倒れるたびに置き場所も消えて使い回せない。
// 同じ種類の敵の全員で 1 つの置き場所を共有するため、敵より長く生きるシーン(GameScene)が持つ。
// 種類ごとに分けるのは、1 つの置き場所には同じ作りの弾だけを入れるため(敵の種類ごとに、モデルや大きさが変わる)。
//
// 責務は「種類ごとの置き場所を渡す」だけ。弾の作り方は知らない(使う人 = 敵の射撃部品が、定義から組み立てる)。
// 部品へは、シーンの型で引く表(ServiceTable)を通して、部品の生成のときに渡す。
class EnemyBulletPools {
public:
  EnemyBulletPools();
  // unique_ptr<GameObjectPool>(不完全な型)を消すため、定義は .cpp に置く
  ~EnemyBulletPools();

  // 置き場所の場所(参照)を部品が持つので、コピーとムーブはしない
  EnemyBulletPools(const EnemyBulletPools&) = delete;
  EnemyBulletPools& operator=(const EnemyBulletPools&) = delete;

  // bulletId の弾の置き場所を返す。初めて求められた id なら、空の置き場所を作る。
  // created: nullptr でなければ、今回新しく作ったかを入れる(作った人が、前もって弾を入れるため)
  AbsoluteEngine::GameObjectPool& Get(const std::string& bulletId, bool* created = nullptr);

private:
  // GameObjectPool はムーブできないので、ポインタで持つ(表が大きくなって並び替わっても、置き場所の場所は変わらない)
  std::unordered_map<std::string, std::unique_ptr<AbsoluteEngine::GameObjectPool>> pools_;
};
