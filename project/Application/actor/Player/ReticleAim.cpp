#include "ReticleAim.h"
#include "GameCamera.h"
#include "Method.h" // Multiply, Inverse
#include "Application/ScreenSize.h"
#include <cmath>

namespace ReticleAim {

Vector3 CursorRayDirection(const GameCamera& camera, const Vector2& cursorPos) {
  // カーソルのスクリーン座標 → NDC
  const float ndcX = (cursorPos.x / ScreenSize::kWidth)  * 2.0f - 1.0f;
  const float ndcY = 1.0f - (cursorPos.y / ScreenSize::kHeight) * 2.0f;

  const Matrix4x4 invVp = Inverse(Multiply(camera.GetViewMatrix(), camera.GetProjectionMatrix()));

  // NDC → ワールド座標
  auto unproject = [&](float z) -> Vector3 {
    const float w = ndcX * invVp.m[0][3] + ndcY * invVp.m[1][3] + z * invVp.m[2][3] + invVp.m[3][3];
    return {
      (ndcX * invVp.m[0][0] + ndcY * invVp.m[1][0] + z * invVp.m[2][0] + invVp.m[3][0]) / w,
      (ndcX * invVp.m[0][1] + ndcY * invVp.m[1][1] + z * invVp.m[2][1] + invVp.m[3][1]) / w,
      (ndcX * invVp.m[0][2] + ndcY * invVp.m[1][2] + z * invVp.m[2][2] + invVp.m[3][2]) / w
    };
  };

  const Vector3 nearPoint = unproject(0.0f);
  const Vector3 farPoint  = unproject(1.0f);
  Vector3 dir = { farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z };

  const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (len <= 0.0001f) {
    return camera.GetForward();
  }
  return { dir.x / len, dir.y / len, dir.z / len };
}

} // namespace ReticleAim
