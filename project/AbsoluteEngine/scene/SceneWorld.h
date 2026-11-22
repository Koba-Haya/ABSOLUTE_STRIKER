#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace AbsoluteEngine {

class GameObject;

// シーン直下のオブジェクト一覧を所有し、その管理(追加・削除・更新・不要物の回収・保存と復元)を担当するクラス。
//
// 分けた理由: BaseScene が「シーンの骨格」と「オブジェクトの管理」を両方持っていて責務が混ざっていたため、
// オブジェクトの管理だけを切り出した(単一責任)。
// 一覧そのもの(std::vector)は外へ書き換え可能な形では公開しない。追加・削除は必ずこのクラスのメソッドを通す。
// これにより、エディタやコマンドが一覧を直接いじって、整合性が崩れることを防ぐ。
class SceneWorld {
public:
  using ObjectList = std::vector<std::shared_ptr<GameObject>>;

  // オブジェクトを一覧の末尾へ追加する。nullptr は無視する
  void Add(std::shared_ptr<GameObject> obj);

  // 指定したオブジェクトを一覧から取り除く(含まれていなければ何もしない)。
  // Objects() の巡回中は呼ばないこと(巡回中に削除するとイテレータが無効になる)
  void Remove(const std::shared_ptr<GameObject>& obj);

  // 条件に合うオブジェクトをまとめて取り除く。shouldRemove は各オブジェクトに対して1回ずつ呼ばれる。
  // 条件の中でこの一覧を変更しないこと
  void RemoveIf(const std::function<bool(const std::shared_ptr<GameObject>&)>& shouldRemove);

  // 全てのオブジェクトを取り除く
  void Clear();

  // 一覧を読み取り専用で返す。保持せず、使うたびに取得すること
  const ObjectList& Objects() const { return objects_; }

  // 世界を1回更新する。有効なオブジェクトの Update を呼んだあと、衝突判定を行う。
  // 更新中に追加されたオブジェクトでループが壊れないよう、先に個数を固定して添字で回す(追加分は次の更新から動く)
  // deltaTime: 世界の経過時間(秒)。スローモーションの係数が掛かった後の値を渡す
  void Update(float deltaTime);

  // 無効(IsActive() == false、または nullptr)になったオブジェクトを一覧から取り除く。
  // GameObject::Destroy() は無効にするだけで、実際の削除はここで行われる
  void CollectGarbage();

  // 現在の一覧の状態を、内部に預ける(再生前のバックアップ用)。前に預けた状態は上書きされる。
  // 保存はフルシリアライズで行う(Memento パターン: 状態を丸ごと預けて、あとで戻す)
  void SaveSnapshot();

  // 預けた状態から一覧を作り直す(再生を止めたときの復元用)。今の一覧は先に全て捨てる。
  // 戻り値: 預けた状態が空、または読み込みに失敗したとき false
  bool RestoreSnapshot();

  // JSON ファイルから一覧を読み込む。ファイルが開けなかったときは、今の一覧を変えずに false を返す
  bool LoadFromFile(const std::string& filePath);

  // JSON ファイルへ一覧を保存する(タイムラインのプレビュー用オブジェクトは保存されない)
  bool SaveToFile(const std::string& filePath) const;

private:
  ObjectList objects_;
  std::string snapshotJson_; // SaveSnapshot で預けた状態(JSON)
};

} // namespace AbsoluteEngine
