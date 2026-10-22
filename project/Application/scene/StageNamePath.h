#pragma once
#include "Vector.h"

// ステージ名の板が、俯瞰の間と、竜を飛び越えて前へ降りるときに通る位置の計算(純粋な数学。ゲームの状態は知らない)。
// 板の「衝撃点」の位置を返す(板を置く位置は、StageNameBoard::Place が衝撃点から決める)。
namespace StageNamePath {

// カメラの正面に置く位置。視線の方向へ distance だけ進み、そこから、視線より angle(ラジアン)だけ上へずらす。
// 画面の上寄りに置くと、カメラの前に出す板が、画面の下にいる竜と重ならない
// eye / target: カメラの位置と注視点。distance: カメラからの距離(ユニット)。angle: 視線からの上向きの角度(ラジアン)
Vector3 InFrontOfCamera(const Vector3 &eye, const Vector3 &target, float distance, float angle);

// 始点から終点へ向かう直線(弦)の上の点。s: 0 = 始点、1 = 終点
Vector3 ChordPoint(const Vector3 &start, const Vector3 &end, float s);

// 弦から、ワールドの上へ持ち上げる高さ(弧の形)。s = 0.5 で最大の height になり、両端で 0 になる放物線
// 弦の上の点に、この高さだけ上を足すと、竜の上を飛び越える弧になる
float ArcLift(float height, float s);

// 点 point に衝撃点を置く板が、画面の上端を超えずに、ワールドの上へ持ち上げられる最大の高さ(ユニット)。
// 弧の高さを、これで抑えると、カメラが動いたり画角が変わったりしても、板が画面の外へ出ない。
// すでに上端を超えている、またはカメラの後ろにあるときは 0
// point: 板の衝撃点の位置。aboveHeight: 衝撃点から板の上端までの高さ(ユニット)
// eye / target / fovY: 画面に映るカメラの位置、注視点、縦の画角(ラジアン)
// limit: 板の上端が届いてよい高さ(画面の半分の高さに対する割合。1 で画面の上端)
float MaxLiftInView(const Vector3 &point, float aboveHeight, const Vector3 &eye, const Vector3 &target, float fovY, float limit);

} // namespace StageNamePath
