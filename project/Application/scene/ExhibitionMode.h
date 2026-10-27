#pragma once
#include "SceneIds.h"

// 展示用の自動巡回の設定。
// kEnabled が true の間は、操作を一切受け付けず(死亡演出のスキップも無効)、次の順に繰り返す:
//   (起動の最初の 1 回だけ: タイトル)-> ゲーム(スタート演出)-> スタート演出が終わって kGameHoldSec 秒後に、自動で死ぬ
//   -> 死亡演出(崩壊、落下、雲)-> 霧と水滴の遷移 -> ゲームオーバー -> ゲーム(スタート演出)-> ...
// 自動で死ぬときは、デバッグのショートカットではなく、本物の死亡処理(GameScene::KillPlayer_ -> Dying)を通す。
// クリアは見せない。自動プレイ(撃つ・避ける)はしない。
// 通常の開発・プレイでは false にする(キー入力で進める従来の動作に戻る)
namespace ExhibitionMode {

constexpr bool kEnabled = true;

// タイトルが表示されてから、自動でスタート演出を始めるまでの時間(秒)。ロゴが出きる時間(2 秒)より長くする
constexpr float kTitleWaitSec = 3.0f;
// スタート演出が終わってから、自機を自動で死なせる(死亡演出を始める)までの時間(秒)。
// レールを飛ぶ様子を、少しだけ見せてから、見せ場(崩壊、落下カメラ、雲、霧と水滴)へ進む
constexpr float kGameHoldSec = 1.0f;
// クリア / ゲームオーバーの覆いが晴れてから、タイトルへの遷移を要求するまでの時間(秒)
constexpr float kResultHoldSec = 3.0f;

} // namespace ExhibitionMode
