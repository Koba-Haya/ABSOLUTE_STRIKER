#pragma once
#include <array>

#include "SceneIds.h"
#include "../item/ItemDefinition.h"

// 展示用の自動巡回の設定(クリア演出の展示)。この名前空間は、時間などの定数だけを持つ。
// 展示モードに入っているか(状態)は、ExhibitionSession が持つ。入っている間は、操作を一切受け付けず、次の順に繰り返す:
//   ゲーム(スタート演出)-> ボス戦へ飛ぶ -> すぐ倒す -> 撃破演出と島の解放 -> クリア画面(固定の結果、継承の自動操作)
//   -> ステージ選択(鍵が外れる演出)-> 自動で決定 -> ゲーム ...
// 展示の間は、保存データを書き込まない(メモリ上の仮の状態で動く。ExhibitionController が、入るときに実際のデータを退避し、出るときに戻す)。
// 周回のたびに、仮の状態を初期へ戻す(島の解放、ベスト記録、継承を、毎周見せるため)。
// 切り替え: 実行中に TAB キーで、入る / 出る(ExhibitionController が ExhibitionSession を書き換える)。起動時は、通常のプレイ。
namespace ExhibitionMode {

// ステージ選択で、鍵が外れる演出が終わってから、カーソルを先頭のステージへ戻すまで / 戻してから決定するまでの時間(秒)
constexpr float kStageSelectHoldSec = 1.2f;
// タイトルが表示されてから、自動でスタート演出を始めるまでの時間(秒)。ロゴが出きる時間(2 秒)より長くする
constexpr float kTitleWaitSec = 3.0f;
// スタート演出が終わってから、ボス戦へ飛ぶまでの時間(秒)。レールを飛ぶ様子を、少しだけ見せる
constexpr float kGameHoldSec = 1.0f;
// ボス戦に入ってから、ボスを倒すまでの時間(秒)
constexpr float kBossHoldSec = 1.0f;
// 結果の集計が終わってから、継承の選択を始めるまでの時間(秒。集計を、ゆっくり見せる)
constexpr float kTallyHoldSec = 2.5f;
// 継承の確定(メニューが開く)から、ステージ選択へ進む要求を出すまでの時間(秒)
constexpr float kResultHoldSec = 3.0f;
// 継承の選択で、自動操作が 1 回動かす間隔(秒)と、1 つの強化の行で選ぶ個数の上限(個数が見える様子を見せる)
constexpr float kPickStepSec = 0.8f;
constexpr int kPickMaxPerRow = 2;
// ゲームオーバー: 覆いが晴れてから、ゲームへ戻る要求を出すまでは「カメラの降下の長さ + この秒数」
// (降下は GameOverBackdrop が持つ。降下が終わって灰が集まりきり、核が灯る姿勢を、少し見せてから、復活の演出(リトライ)を始める)
constexpr float kGameOverHoldMarginSec = 1.5f;

// 展示で、プレイの最初から持たせる強化(継承の選択に、候補を出すため。ランクで枠が変わる(S 3、A 2、B 2、C 1)ので、枠より多めに持たせる)
struct InitialUpgrade {
  ItemEffectKind kind;
  int count;
};
constexpr std::array<InitialUpgrade, 4> kInitialUpgrades = {{
    {ItemEffectKind::LockOnMax, 2},
    {ItemEffectKind::MaxHp, 2},
    {ItemEffectKind::FireRate, 1},
    {ItemEffectKind::LockRadius, 1},
}};

} // namespace ExhibitionMode
