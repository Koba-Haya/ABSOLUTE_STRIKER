#pragma once
#include "Type/Vector.h"

// ボスの誘導弾の玉の、瞬間的な演出(発射、飛行中の火花、着弾、撃墜)。粒を出すだけで、状態を持たない。
//
// 分けた理由: 瞬間の演出を、玉の見た目の部品(BossOrbVisualComponent。描画と尾の処理)と分ける。
// 玉は、終わり方の違い(着弾、撃墜)ごとに、別の演出を出す。
// yuudou.gif を参考にした見せ方:
//   発射     : 構えで溜めた力が、1 点から弾ける。強い星形の閃光(十字の光)、進む向きへ伸びる光の筋、衝撃波の輪、後ろへ飛ぶ火花
//   飛行中   : 後ろへ飛ぶ細長い火花(進む向きが、画面の上でも分かる)
//   着弾     : 自機に着いたとき。中心の閃光、星形の閃光、放射状の火花、衝撃波の輪、火の粉
//   撃墜     : 撃ち落とされたとき。着弾より白く鋭い閃光、輪を 2 重、多めの火花
// 粒は、ボスの攻撃の演出と同じグループ(BossEffectParticles。アルファ合成で、雲のあとに描く)に出す。
// 共通の引数: position: 演出の位置(ワールド)。right / up: 画面の右と上の向き(粒を、画面に平行な面へ出すための、カメラの向き。長さ 1)。
namespace BossOrbEffects {
// 発射の瞬間。direction: 飛んでいく向き(長さ 1)
void EmitLaunch(const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up);

// 飛行中の火花を count 個、玉の後ろへ出す(毎フレーム、数を決めて呼ぶ)。direction: 飛んでいる向き(長さ 1)
void EmitFlightSparks(const Vector3& position, const Vector3& direction, const Vector3& right, const Vector3& up, int count);

// 自機に着いたとき(または島に遮られたとき)
void EmitImpact(const Vector3& position, const Vector3& right, const Vector3& up);

// 自機の弾に撃ち落とされたとき
void EmitShotDown(const Vector3& position, const Vector3& right, const Vector3& up);
} // namespace BossOrbEffects
