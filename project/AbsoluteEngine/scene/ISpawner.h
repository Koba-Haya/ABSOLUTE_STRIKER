#pragma once
#include <memory>

namespace AbsoluteEngine {

class GameObject;

// 「オブジェクトをシーンへ出す」機能だけを切り出した窓口(インターフェース)。
//
// 作った理由: タイムラインのイベントや敵の部品が、弾や敵を出すために「今のシーン全体」を
// グローバル(BaseScene::GetActiveScene)から取っていた。出したいだけなのに、シーンの全機能に触れる状態で、
// しかも依存がコードに見えなかった。必要な機能(出す)だけをこの窓口にして、使う側が受け取る形にする
// (インターフェース分離の原則、依存性の逆転の原則)。
// 実装はシーン(BaseScene)が持つ。使う側はシーンの型を知らなくてよい。
class ISpawner {
public:
  virtual ~ISpawner() = default;

  // オブジェクトをシーンへ出す。nullptr は無視する。
  // 実装側が、追加時の追加処理(コールバックの注入など)を行うことがある
  virtual void Spawn(std::shared_ptr<GameObject> obj) = 0;
};

} // namespace AbsoluteEngine
