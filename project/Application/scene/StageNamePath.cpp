#include "StageNamePath.h"

#include "Method.h"

#include <algorithm>
#include <cmath>

namespace {
// 外積の長さがこれ以下のときは、右向きが決まらない(真上・真下を向いている)ので、ワールドの x 軸を使う
constexpr float kMinRightLength = 1e-4f;
// 放物線の係数。s(1 - s) は s = 0.5 で最大 0.25 なので、4 倍すると最大が 1 になる
constexpr float kParabolaScale = 4.0f;
// 画面に映る奥行き(カメラの前方向の距離)が、これ以下なら、カメラの後ろ(または真横)にあるとみなす
constexpr float kMinDepth = 1e-3f;
// カメラの上向きとワールドの上向きの内積が、これ以下のときは、ワールドの上へ持ち上げても画面の上へは動かない
constexpr float kMinUpDot = 0.2f;

// カメラの右と上(左手系: 上 x 前 = 右、前 x 右 = 上)
void CameraAxes(const Vector3 &forward, Vector3 &outRight, Vector3 &outUp) {
  const Vector3 worldUp{0.0f, 1.0f, 0.0f};
  Vector3 right = Cross(worldUp, forward);
  outRight = Length(right) > kMinRightLength ? Normalize(right) : Vector3{1.0f, 0.0f, 0.0f};
  outUp = Cross(forward, outRight);
}
} // namespace

namespace StageNamePath {

Vector3 InFrontOfCamera(const Vector3 &eye, const Vector3 &target, float distance, float angle) {
  const Vector3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
  Vector3 right{};
  Vector3 up{};
  CameraAxes(forward, right, up);

  // 視線の正面 distance の位置から、上へ distance * tan(angle) ずらす(視線より angle だけ上に見える位置)
  const float lift = distance * std::tan(angle);
  return {eye.x + forward.x * distance + up.x * lift, eye.y + forward.y * distance + up.y * lift,
          eye.z + forward.z * distance + up.z * lift};
}

Vector3 ChordPoint(const Vector3 &start, const Vector3 &end, float s) {
  return {start.x + (end.x - start.x) * s, start.y + (end.y - start.y) * s, start.z + (end.z - start.z) * s};
}

float ArcLift(float height, float s) {
  const float c = std::clamp(s, 0.0f, 1.0f);
  return height * kParabolaScale * c * (1.0f - c);
}

float MaxLiftInView(const Vector3 &point, float aboveHeight, const Vector3 &eye, const Vector3 &target, float fovY, float limit) {
  const Vector3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
  Vector3 right{};
  Vector3 up{};
  CameraAxes(forward, right, up);

  const Vector3 d{point.x - eye.x, point.y - eye.y, point.z - eye.z};
  const float depth = Dot(d, forward);
  if (depth <= kMinDepth) {
    return 0.0f;
  }

  // 板の上端が、画面の中でどの高さにあるか(-1 = 下端、1 = 上端)。
  // 奥行き depth のところでは、画面の半分の高さが depth * tan(fovY / 2) のユニットに当たる
  const float halfViewHeight = depth * std::tan(fovY * 0.5f);
  const float topNdc = (Dot(d, up) + aboveHeight) / halfViewHeight;

  // 上端が limit に届くまでの余裕(ユニット)。ワールドの上へ持ち上げたぶんのうち、画面の上へ動くのは、カメラの上向きとの内積の割合
  const float room = (limit - topNdc) * halfViewHeight;
  const float upDot = std::max(up.y, kMinUpDot);
  return std::max(0.0f, room / upDot);
}

} // namespace StageNamePath
