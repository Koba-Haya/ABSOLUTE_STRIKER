#pragma once

namespace AbsoluteEngine {

class ICameraRig;
class IScreenView;
class ISpawner;

// 部品(IComponent)を作るときに、部品へ渡せる「窓口の束」。
//
// 作った理由: 部品が、弾や敵を出したり、カメラの位置を知ったりするために、グローバル(BaseScene::GetActiveScene)から
// 「今のシーン」を探していた。部品は、ファイル(JSON)の読み込みや、エディタの「部品を追加」でも作られる。
// そこで、作る側(ComponentFactory を呼ぶ側)が必要な窓口をこの束にして渡し、Factory が部品のコンストラクタへ配る形にした
// (コンストラクタ注入)。GameObject は、自分の持ち主(シーン)を知らない作りのまま。窓口は、部品だけが受け取る。
//
// 窓口が増えたら、ここへ足す(例: シーンへの問い合わせ)。
// 所有しない(ポインタの実体は、シーンが持つ)。束を保持する場合は、窓口の実体より長く生きないこと。
struct ComponentDeps {
  // オブジェクトを出す窓口。弾、敵、エフェクトなどを出す部品が使う
  ISpawner* spawner = nullptr;
  // 画面(メインカメラ)の見え方を、読み取り専用で知る窓口。画面内か、カメラの位置や向きを知る部品が使う
  IScreenView* screen = nullptr;
  // メインカメラの位置と向きを書き換える窓口。レールカメラの部品だけが使う
  ICameraRig* cameraRig = nullptr;

  // 窓口がすべてそろっているか。窓口を要する部品を作る前に確かめる(そろっていなければ、作らない)
  bool IsComplete() const { return spawner != nullptr && screen != nullptr && cameraRig != nullptr; }
};

} // namespace AbsoluteEngine
