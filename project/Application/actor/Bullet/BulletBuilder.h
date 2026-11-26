#pragma once
#include <functional>
#include <memory>

#include "Type/Vector.h"

namespace AbsoluteEngine {
class GameObject;
class GameObjectPool;
}
struct BulletDefinition;

// 弾の定義(BulletDefinition)から、まっすぐ飛ぶ弾の GameObject を組み立てる関数と、撃つたびに値を設定する関数。
//
// 分けた理由: 組み立て(1 回だけ。部品とモデルを付ける)と、発射(撃つたび。位置と速度を入れる)を分けておくと、
// 使い終わった弾を使い回す(Object Pool)ときに、借りた弾へ発射だけをやり直せばよい。
// 同じ定義からは、誰が組み立てても同じ作りの弾になる(定義だけで決まる)ので、複数の部品が 1 つの置き場所を共有しても、作りが混ざらない。
namespace BulletBuilder {

// 定義 def の弾を組み立てる(当たり判定の球、BulletComponent、定義にあればモデル)。
// tag: どちらの陣営の弾か(BulletTag の値)。位置と速度は入っていないので、出す前に Launch を呼ぶこと
std::unique_ptr<AbsoluteEngine::GameObject> Build(const BulletDefinition &def, const char *tag);

// 組み立て済みの弾 bullet に、今回の位置と速度を入れ、寿命の経過を 0 に戻す(新しく作った弾にも、使い回す弾にも呼ぶ)
void Launch(AbsoluteEngine::GameObject &bullet, const Vector3 &position, const Vector3 &velocity);

// 定義の部品に加えて、使う人が弾に付ける部品を足す関数(例: ボスの弾の見た目)。空なら、何も足さない。
// 組み立てのとき(新しく作るときだけ)に 1 回呼ばれる。使い回すときは呼ばれないので、撃つたびの値は、使う人が設定し直すこと
using ExtraPartsAdder = std::function<void(AbsoluteEngine::GameObject &bullet)>;

// 置き場所 pool から、定義 def の弾を 1 つ借りる。空きが無ければ、Build で新しく組み立て(addParts も呼ぶ)、pool の管理に入れる。
// 戻り値は、手放されたら pool へ戻る。位置と速度は入っていないので、出す前に Launch を呼ぶこと。
// pool には、同じ def と tag と addParts の弾だけを入れること(借りた弾は、組み立て直さずに使うため)
std::shared_ptr<AbsoluteEngine::GameObject> AcquireOrBuild(AbsoluteEngine::GameObjectPool &pool, const BulletDefinition &def, const char *tag,
                                                           const ExtraPartsAdder &addParts = {});

// 定義 def の prewarmCount の数だけ弾を組み立てて(addParts も呼ぶ)、空きとして pool へ入れておく(持ち主の生成のときに 1 回呼ぶ)
void Prewarm(AbsoluteEngine::GameObjectPool &pool, const BulletDefinition &def, const char *tag, const ExtraPartsAdder &addParts = {});

} // namespace BulletBuilder
