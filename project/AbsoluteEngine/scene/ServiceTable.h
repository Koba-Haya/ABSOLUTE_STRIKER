#pragma once
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace AbsoluteEngine {

// シーンが持つ「ゲーム固有の物」を、型で引く表(シーンごとに 1 つ。SceneWorld が持ち、ComponentDeps の services で渡す)。
//
// 作った理由: 部品へ物を渡す窓口の束(ComponentDeps)はエンジン側にあるので、ゲーム固有の型(弾の定義の表など)を
// 項目として足すと、エンジンがゲームを知ってしまう(依存が逆向きになる)。この表は、型を引数にした汎用の入れ物なので、
// エンジンは中身の型を知らないまま、ゲーム側の物を部品の生成まで運べる。
//
// 決まり(Service Locator の欠点「どこからでも引けて、依存が隠れる」を避けるため):
// - 表はシーンごとに 1 つで、グローバルではない。
// - 表から引くのは、部品を作る関数(ComponentFactory への登録の関数)の中だけにする。
//   部品そのものは、今までどおりコンストラクタで参照を受け取り、この表を知らない(依存がコンストラクタの引数に見える)。
//
// 型の区別: const の有無も別の型として扱う。Register<const X> したものは、Find<const X> でしか引けない
// (読み取り専用で登録した物を、書き換えできる形で取り出せないようにするため)。
// 所有しない。登録した物は、この表(= シーン)より長く生きること。
class ServiceTable {
public:
  // service を、型 T で登録する。同じ型がすでにあれば、置き換える
  template <typename T>
  void Register(T& service) {
    entries_[Key_<T>()] = const_cast<void*>(static_cast<const void*>(&service));
  }

  // 型 T の登録を外す(無ければ何もしない)
  template <typename T>
  void Unregister() {
    entries_.erase(Key_<T>());
  }

  // 型 T で登録された物を返す。無ければ nullptr
  template <typename T>
  T* Find() const {
    const auto it = entries_.find(Key_<T>());
    if (it == entries_.end()) {
      return nullptr;
    }
    // 登録のときと同じ型 T で引いているので、元の型へ戻してよい
    return static_cast<T*>(it->second);
  }

private:
  // 表の鍵。T そのものではなく T* の型情報を使う(typeid は一番外側の const を無視するが、
  // const X* と X* は別の型として区別されるため)
  template <typename T>
  static std::type_index Key_() {
    return std::type_index(typeid(T*));
  }

  std::unordered_map<std::type_index, void*> entries_;
};

} // namespace AbsoluteEngine
