#pragma once
#include <memory>
#include <vector>

namespace AbsoluteEngine {

class GameObject;

// 「いまシーンにあるオブジェクト」を、読み取り専用で調べるための窓口(インターフェース)。
//
// 作った理由: 敵やアイテムの部品が、自機を探すために、グローバル(以前の BaseScene::GetActiveScene)から
// 「今のシーン」を取り、その全オブジェクトを自分でたどっていた(同じ探し方が 7 か所に複製されていた)。
// 知りたいのは「オブジェクトの一覧」だけなので、その機能だけを窓口にして、使う側が受け取る形にする。
// 読み取り専用なので、この窓口を受け取った部品は、オブジェクトを追加・削除できない(追加は ISpawner)。
// 実装は SceneWorld(一覧の持ち主)。
//
// エンジン側のこの窓口は、「自機」などゲーム固有の型を知らない。自機を探す処理は、ゲーム側に 1 か所にまとめる(PlayerFinder)。
class ISceneQuery {
public:
  using ObjectList = std::vector<std::shared_ptr<GameObject>>;

  virtual ~ISceneQuery() = default;

  // シーン直下のオブジェクトの一覧を返す。nullptr が入っていることがあるので、使う側で確かめること。
  // 保持せず、使うたびに取得すること(追加で、中身が変わる)。巡回中に ISpawner で追加しないこと
  virtual const ObjectList& Objects() const = 0;
};

} // namespace AbsoluteEngine
