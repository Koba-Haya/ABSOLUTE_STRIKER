#pragma once
#include "Method.h"

// 行を (右, 上, 前, 位置) として並べた行列。モデルの x -> 右、y -> 上、z -> 前 に対応する。
// 右と上のベクトルの長さが、そのまま拡大率になるので、板の大きさもここで決められる
inline Matrix4x4 MakeBasisMatrix(const Vector3 &right, const Vector3 &up, const Vector3 &forward, const Vector3 &position) {
  Matrix4x4 m = MakeIdentity4x4();
  m.m[0][0] = right.x;    m.m[0][1] = right.y;    m.m[0][2] = right.z;
  m.m[1][0] = up.x;       m.m[1][1] = up.y;       m.m[1][2] = up.z;
  m.m[2][0] = forward.x;  m.m[2][1] = forward.y;  m.m[2][2] = forward.z;
  m.m[3][0] = position.x; m.m[3][1] = position.y; m.m[3][2] = position.z;
  return m;
}
