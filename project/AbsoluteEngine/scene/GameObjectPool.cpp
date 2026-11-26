#include "GameObjectPool.h"

#include "GameObject.h"

namespace AbsoluteEngine {

GameObjectPool::GameObjectPool() : storage_(std::make_shared<Storage>()) {}

// Storage の中の unique_ptr<GameObject> を消すため、GameObject の定義が見えるここで定義する
GameObjectPool::~GameObjectPool() = default;

std::shared_ptr<GameObject> GameObjectPool::TryAcquire() {
  if (storage_->idle.empty()) {
    return nullptr;
  }
  // 最後に戻った物から貸す(vector の末尾から取り出すのが一番軽いため。どれを貸しても作りは同じ)
  std::unique_ptr<GameObject> obj = std::move(storage_->idle.back());
  storage_->idle.pop_back();
  // 前回 Destroy() で落とした生存フラグを戻す(戻さないと、シーンの一覧に入れた直後に、不要物として外される)
  obj->Reactivate();
  return Lend_(storage_, std::move(obj));
}

std::shared_ptr<GameObject> GameObjectPool::Adopt(std::unique_ptr<GameObject> obj) {
  if (!obj) {
    return nullptr;
  }
  return Lend_(storage_, std::move(obj));
}

void GameObjectPool::AddIdle(std::unique_ptr<GameObject> obj) {
  if (!obj) {
    return;
  }
  storage_->idle.push_back(std::move(obj));
}

std::size_t GameObjectPool::GetIdleCount() const {
  return storage_->idle.size();
}

std::shared_ptr<GameObject> GameObjectPool::Lend_(const std::shared_ptr<Storage>& storage, std::unique_ptr<GameObject> obj) {
  // デリーターは置き場所を weak_ptr で持つ(置き場所の寿命を延ばさない。置き場所が先に消えたかを確かめられる)
  std::weak_ptr<Storage> weakStorage = storage;
  auto returnToPool = [weakStorage](GameObject* raw) {
    std::unique_ptr<GameObject> owned(raw);
    if (auto alive = weakStorage.lock()) {
      // 置き場所がまだあるので、削除せずに空きとして戻す
      alive->idle.push_back(std::move(owned));
    }
    // 置き場所が無ければ、owned がここで普通に削除する
  };
  // 生ポインタから shared_ptr を作ると、GameObject の enable_shared_from_this も、この新しい shared_ptr を指し直す
  // (前回の shared_ptr は、戻った時点で全部消えているため)
  return std::shared_ptr<GameObject>(obj.release(), returnToPool);
}

} // namespace AbsoluteEngine
