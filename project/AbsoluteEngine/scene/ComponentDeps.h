#pragma once

namespace AbsoluteEngine {

class ISpawner;

// 部品(IComponent)を作るときに、部品へ渡せる「窓口の束」。
//
// 作った理由: 部品が、弾や敵を出すために、グローバル(BaseScene::GetActiveScene)から「今のシーン」を探していた。
// 部品は、ファイル(JSON)の読み込みや、エディタの「部品を追加」でも作られる。そこで、作る側(ComponentFactory を呼ぶ側)が
// 必要な窓口をこの束にして渡し、Factory が部品のコンストラクタへ配る形にした(コンストラクタ注入)。
// GameObject は、自分の持ち主(シーン)を知らない作りのまま。窓口は、部品だけが受け取る。
//
// 窓口が増えたら、ここへ足す(例: 画面の情報、シーンへの問い合わせ)。
// 所有しない(ポインタの実体は、シーンが持つ)。束を保持する場合は、窓口の実体より長く生きないこと。
// 窓口が nullptr の束は、その窓口を要する部品を作れない(作る側が、必ず設定した束を渡す)
struct ComponentDeps {
  // オブジェクトを出す窓口。弾、敵、エフェクトなどを出す部品が使う
  ISpawner* spawner = nullptr;
};

} // namespace AbsoluteEngine
