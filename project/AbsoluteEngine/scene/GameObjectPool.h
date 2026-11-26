#pragma once
#include <cstddef>
#include <memory>
#include <vector>

namespace AbsoluteEngine {

class GameObject;

// 使い終わった GameObject を捨てずに預かり、次に使う人へ貸し出す置き場所(Object Pool パターン)。
//
// 作った理由: 弾のように、短い間に何度も作っては消す物は、作るたびに部品やモデル(GPU のバッファ)を確保し、
// 消すたびに解放していた。使い終わった物を取っておいて使い回せば、その確保と解放をしなくて済む。
//
// 責務は「置き場所」だけ。作り方は知らない(何の部品を持つか、どのモデルかは、使う人が知っている)。
// 使い方:
//   1. TryAcquire() で、空いている物を借りる。空きが無ければ nullptr が返る
//   2. nullptr なら、使う人が自分で組み立てて、Adopt() でこの置き場所の管理に入れる
//   3. どちらの場合も、使う人が今回の値(位置、速度など)を設定し直してから、シーンへ出す(前回の値が残っているため)
//   4. 返却は書かなくてよい。シーンの一覧などの持ち主が全員手放した(最後の shared_ptr が消えた)ときに、自動で戻る
//
// 自動で戻る仕組み: 貸し出すときに、shared_ptr の「削除の仕方」(デリーター)を、「削除せずに置き場所へ戻す」に差し替える。
// 物の側(弾の部品など)は、今までどおり GameObject::Destroy() するだけで、置き場所を知らなくてよい。
// 貸し出すたびに新しい shared_ptr を作るので、前の借り手が持っていた weak_ptr は、戻った時点で切れる
// (使い回された物を、前の借り手が「自分の物がまだ生きている」と取り違えない)。
//
// 注意:
// - 1 つの置き場所には、同じ作りの物だけを入れること。借りた人は、組み立て済みの部品があることを前提に、値だけを設定し直すため。
// - この置き場所が先に消えても安全。そのあとに手放された物は、戻らずに普通に削除される。
// - 1 つのスレッドから使う前提(ロックしない)。
class GameObjectPool {
public:
  GameObjectPool();
  ~GameObjectPool();

  // 預かっている物を指す shared_ptr のデリーターが、中身を共有するため、コピーとムーブはしない
  GameObjectPool(const GameObjectPool&) = delete;
  GameObjectPool& operator=(const GameObjectPool&) = delete;
  GameObjectPool(GameObjectPool&&) = delete;
  GameObjectPool& operator=(GameObjectPool&&) = delete;

  // 空いている物を 1 つ借りる。空きが無ければ nullptr。
  // 借りた物は、生存フラグを戻してある(GameObject::Reactivate 済み)。それ以外の値(位置、部品の状態)は前回のまま
  std::shared_ptr<GameObject> TryAcquire();

  // 使う人が新しく組み立てた物を、この置き場所の管理に入れて、貸し出し中の状態で返す。
  // 返り値をシーンへ出せば、手放されたときに、この置き場所へ戻る。obj が nullptr なら nullptr を返す
  std::shared_ptr<GameObject> Adopt(std::unique_ptr<GameObject> obj);

  // 組み立てた物を、貸し出さずに、空きとして預ける(シーンの開始時に、前もって作っておくときに使う)。
  // obj が nullptr なら何もしない
  void AddIdle(std::unique_ptr<GameObject> obj);

  // いま空いている(貸し出していない)物の数(開発用の表示や確認に使う)
  std::size_t GetIdleCount() const;

private:
  // 空いている物の置き場。貸し出し中の物のデリーターが weak_ptr で指すため、shared_ptr で持つ
  // (置き場所が消えたら weak_ptr が切れ、デリーターは戻さずに削除する)
  struct Storage {
    std::vector<std::unique_ptr<GameObject>> idle;
  };

  // obj を、手放されたら storage へ戻る shared_ptr で包む
  static std::shared_ptr<GameObject> Lend_(const std::shared_ptr<Storage>& storage, std::unique_ptr<GameObject> obj);

  std::shared_ptr<Storage> storage_;
};

} // namespace AbsoluteEngine
