#pragma once
#include "Type/Vector.h"

class GameCamera;

// カーソル（スクリーン座標）から3D空間上の照準を求める計算をまとめたもの。
namespace ReticleAim {

// カーソル位置を通るカメラレイの単位方向ベクトル（ワールド空間）。
// 逆投影に失敗した場合はカメラの前方向を返す。
Vector3 CursorRayDirection(const GameCamera& camera, const Vector2& cursorPos);

} // namespace ReticleAim
