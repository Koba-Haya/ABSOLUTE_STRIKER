#pragma once

// スタート演出のデモ(提出・録画用)の設定。
// kEnabled が true の間は、タイトルからゲームへは通常どおり操作で進み、
// ゲームでは、スタート演出が終わってから一定時間後に、自動でタイトルへ戻る。
// ゲームを通しで遊ばせず、スタート演出だけを何度も見せるための仕組み。
// 演出が終わっても自機の操作は受け付けない(戻るまでの間に、敵に当たって終わらせないため)。
// ExhibitionMode(タイトルから自動で巡回する展示用)とは別の仕組みなので、同時には使わない。
// 通常の開発・プレイでは false にする
namespace StageStartDemoMode {

constexpr bool kEnabled = true;

// スタート演出が終わってから、タイトルへの遷移を要求するまでの時間(秒)
constexpr float kReturnToTitleDelaySec = 1.0f;

} // namespace StageStartDemoMode
