#pragma once
#include "SceneIds.h"
#include <string>

// 展示用の自動巡回の設定。
// kEnabled が true の間は、操作を一切受け付けず、次の順に画面遷移を繰り返す:
//   タイトル -> ゲーム -> クリア -> タイトル -> ゲーム -> ゲームオーバー -> タイトル -> ...
// 遷移の演出だけを見せるための仕組みなので、ゲームは遷移の覆いが晴れたらすぐ次へ進む。
// 通常の開発・プレイでは false にする(キー入力で進める従来の動作に戻る)
namespace ExhibitionMode {

constexpr bool kEnabled = false;

// タイトルが表示されてから、自動でスタート演出を始めるまでの時間(秒)。ロゴが出きる時間(2 秒)より長くする
constexpr float kTitleWaitSec = 3.0f;
// ゲームの覆いが晴れてから、クリア / ゲームオーバーへの遷移を要求するまでの時間(秒)。0 で、すぐ次へ進む。
// 遷移の演出の余韻(雲を抜けた先の景色)を見せるための間
constexpr float kGameHoldSec = 3.0f;
// クリア / ゲームオーバーの覆いが晴れてから、タイトルへの遷移を要求するまでの時間(秒)
constexpr float kResultHoldSec = 3.0f;

// ゲームの終わり方を、クリアとゲームオーバーで交互に返す(呼ぶたびに次へ進む)。
// 2 つの遷移の演出(ズームブラー / 波紋)を、順番に見せるため
inline const std::string &NextGameEndSceneId() {
  static bool nextIsClear = true;
  const bool useClear = nextIsClear;
  nextIsClear = !nextIsClear;
  return useClear ? SceneId::Clear : SceneId::GameOver;
}

} // namespace ExhibitionMode
