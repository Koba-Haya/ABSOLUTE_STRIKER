#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ComponentDeps.h"
#include "ISceneQuery.h"
#include "ISpawner.h"
#include "CollisionManager.h"
#include "SerialNumberIssuer.h"
#include "ServiceTable.h"

namespace AbsoluteEngine {

class GameObject;

// シーン直下のオブジェクト一覧を所有し、その管理(追加・削除・更新・不要物の回収・保存と復元)を担当するクラス。
//
// 分けた理由: BaseScene が「シーンの骨格」と「オブジェクトの管理」を両方持っていて責務が混ざっていたため、
// オブジェクトの管理だけを切り出した(単一責任)。
// 一覧そのもの(std::vector)は外へ書き換え可能な形では公開しない。追加・削除は必ずこのクラスのメソッドを通す。
// これにより、エディタやコマンドが一覧を直接いじって、整合性が崩れることを防ぐ。
//
// ISpawner(オブジェクトを出す窓口)も実装する。「出す」は、一覧への追加に加えて、追加後の通知(SetOnSpawned)を行う点が、
// 単なる Add と違う。通知は、ゲーム固有の追加処理(敵へのコールバック注入など)を、世界の側が知らずに済むための仕組み(Observer)。
class SceneWorld : public ISpawner, public ISceneQuery {
public:
  // 束(deps_)の、出す窓口と、調べる窓口は、この世界自身
  // 通し番号の発行役(serials_)と、ゲーム固有の物を型で引く表(services_)も、この世界が持ち、束に結びつける
  SceneWorld() {
    deps_.sceneQuery = this;
    deps_.serialNumbers = &serials_;
    deps_.services = &services_;
  }
  // 窓口の束(deps_)が、この世界自身を指すため、コピーとムーブは禁止する(コピー先の束が、コピー元を指してしまうのを防ぐ)
  SceneWorld(const SceneWorld&) = delete;
  SceneWorld& operator=(const SceneWorld&) = delete;

  using ObjectList = std::vector<std::shared_ptr<GameObject>>;
  // 出されたオブジェクトを受け取る関数の型
  using SpawnedCallback = std::function<void(const std::shared_ptr<GameObject>&)>;

  // オブジェクトを一覧の末尾へ追加する。nullptr は無視する。追加の通知(SetOnSpawned)は行わない
  void Add(std::shared_ptr<GameObject> obj);

  // ISpawner の実装。オブジェクトを一覧へ追加し、そのあと、SetOnSpawned で登録された関数を呼ぶ。nullptr は無視する。
  // 実行中に出てくる弾や敵は、この経路を通る。エディタの追加や、ファイルからの読み込みは通らない(通知されない)
  void Spawn(std::shared_ptr<GameObject> obj) override;

  // Spawn でオブジェクトが追加された直後に呼ぶ関数を登録する(1つだけ。空の関数で解除)。
  // 登録した側(シーン)が、この世界より長く生きている間だけ有効な参照を、関数が持つこと
  void SetOnSpawned(SpawnedCallback callback) { onSpawned_ = std::move(callback); }

  // 指定したオブジェクトを一覧から取り除く(含まれていなければ何もしない)。
  // Objects() の巡回中は呼ばないこと(巡回中に削除するとイテレータが無効になる)
  void Remove(const std::shared_ptr<GameObject>& obj);

  // 条件に合うオブジェクトをまとめて取り除く。shouldRemove は各オブジェクトに対して1回ずつ呼ばれる。
  // 条件の中でこの一覧を変更しないこと
  void RemoveIf(const std::function<bool(const std::shared_ptr<GameObject>&)>& shouldRemove);

  // 全てのオブジェクトを取り除く
  void Clear();

  // この世界が持つ、部品へ渡す窓口の束を返す(ISpawner はこの世界自身)。
  // プレハブの読み込みや、部品の生成など、部品を作る場面で、作る側が受け取って使う。この世界が生きている間だけ有効
  const ComponentDeps& Deps() const { return deps_; }

  // 束へ、画面の窓口と、カメラを書き換える窓口を設定する(所有しない)。カメラを持つ側(BaseScene)が、生成のときに 1 回呼ぶ。
  // 設定した窓口は、この世界より長く生きること
  void SetScreenView(IScreenView* screen) { deps_.screen = screen; }
  void SetCameraRig(ICameraRig* rig) { deps_.cameraRig = rig; }
  // 束へ、再生状態の窓口と、入力を設定する(所有しない)。再生状態は BaseScene の生成のとき、入力は Initialize のときに設定する
  void SetPlayState(IPlayState* playState) { deps_.playState = playState; }
  void SetInput(Input* input) { deps_.input = input; }

  // ゲーム固有の物を型で引く表(束の services が指す)。シーンが、部品を作る前に登録する
  ServiceTable& Services() { return services_; }

  // 一覧を読み取り専用で返す。保持せず、使うたびに取得すること
  const ObjectList& Objects() const override { return objects_; }

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
  SpawnedCallback onSpawned_; // Spawn の後に呼ぶ関数(無ければ空)
  CollisionManager collision_; // 衝突判定と押し戻し(Update のたびに使う)
  SerialNumberIssuer serials_; // 通し番号の発行役(束の serialNumbers が指す)
  ServiceTable services_;      // ゲーム固有の物を型で引く表(束の services が指す)
  ComponentDeps deps_{this};  // 部品へ渡す窓口の束(出す窓口と、調べる窓口は、この世界自身。ほかは、持ち主が設定する)
  std::string snapshotJson_; // SaveSnapshot で預けた状態(JSON)
};

} // namespace AbsoluteEngine
