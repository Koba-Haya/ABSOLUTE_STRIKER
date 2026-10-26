#include "DeathCameraDirector.h"
#include <algorithm>
#include <cmath>

namespace {
// 0 〜 1 の範囲の 3 次エルミート補間(p0 から p1 へ。m0, m1 は、その区間の長さに換算した接線)
float Hermite(float p0, float p1, float m0, float m1, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return (2.0f * t3 - 3.0f * t2 + 1.0f) * p0 + (t3 - 2.0f * t2 + t) * m0 + (-2.0f * t3 + 3.0f * t2) * p1 + (t3 - t2) * m1;
}

// キーの値の列から、時刻 time の値を求める。区間の両端の接線は、前後のキーから求める(Catmull-Rom 風)ので、
// キーの時刻でカメラが止まらず、なめらかにつながる
float Sample(const float* values, const float* times, int count, float time) {
  if (time <= times[0]) return values[0];
  if (time >= times[count - 1]) return values[count - 1];
  int i = 0;
  while (i + 1 < count - 1 && time > times[i + 1]) ++i;
  const float span = std::max(times[i + 1] - times[i], 1e-4f);
  const float t = (time - times[i]) / span;

  auto tangent = [&](int k) {
    const int a = std::max(k - 1, 0);
    const int b = std::min(k + 1, count - 1);
    const float dt = std::max(times[b] - times[a], 1e-4f);
    return (values[b] - values[a]) / dt * span; // 区間の長さに換算
  };
  return Hermite(values[i], values[i + 1], tangent(i), tangent(i + 1), t);
}
} // namespace

void DeathCameraDirector::Begin(const DeathCameraSettings& settings, const Vector3& startEye, const Vector3& startTarget,
                                const Vector3& startFocus, const std::array<float, DeathCameraDefaults::kKeyCount>& keyTimes) {
  (void)startTarget;
  settings_ = settings;
  keyTimes_ = keyTimes;
  elapsed_ = 0.0f;
  focus_ = startFocus;
  active_ = true;

  // 死亡の瞬間のカメラを、蝶を中心にした極座標へ直す
  const Vector3 offset{startEye.x - startFocus.x, startEye.y - startFocus.y, startEye.z - startFocus.z};
  const float horizontal = std::sqrt(offset.x * offset.x + offset.z * offset.z);
  start_.distance = std::max(std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z), 0.1f);
  start_.azimuth = std::atan2(offset.x, offset.z);
  start_.elevation = std::atan2(offset.y, std::max(horizontal, 1e-4f));
  start_.fovScale = 1.0f;

  // キーの時刻は、昇順にそろえる(表を調整したときに、逆転しても壊れないように)
  for (size_t i = 1; i < keyTimes_.size(); ++i) keyTimes_[i] = std::max(keyTimes_[i], keyTimes_[i - 1] + 1e-3f);
  Update(0.0f, startFocus);
}

DeathCameraDirector::Spherical DeathCameraDirector::Evaluate_(float time) const {
  constexpr int count = DeathCameraDefaults::kKeyCount;
  float azimuth[count], elevation[count], distance[count], fov[count];
  for (int i = 0; i < count; ++i) {
    const DeathCameraDefaults::Key& key = settings_.keys[i];
    if (i == 0) {
      azimuth[i] = start_.azimuth;
      elevation[i] = start_.elevation;
      distance[i] = start_.distance;
      fov[i] = 1.0f;
    } else {
      azimuth[i] = start_.azimuth + key.azimuthDelta;
      // キー 1 の仰角は、死亡の瞬間のまま(表の値が kKeepElevation のとき)
      elevation[i] = (i == 1 && key.elevation == DeathCameraDefaults::kKeepElevation) ? start_.elevation : key.elevation;
      distance[i] = key.distance;
      fov[i] = key.fovScale;
    }
  }
  float times[count];
  for (int i = 0; i < count; ++i) times[i] = keyTimes_[i];

  Spherical s;
  s.azimuth = Sample(azimuth, times, count, time);
  s.elevation = std::min(Sample(elevation, times, count, time), DeathCameraDefaults::kMaxElevation);
  s.distance = std::max(Sample(distance, times, count, time), 0.5f);
  s.fovScale = std::max(Sample(fov, times, count, time), 0.1f);
  return s;
}

void DeathCameraDirector::Update(float deltaTime, const Vector3& butterflyPosition, float spread) {
  if (!active_) return;
  elapsed_ += deltaTime;

  // 注視点は、蝶の位置へ、指数的に追いつく(フレーム間隔に依存しない式)。最初の更新(時間 0)は、動かさない
  const float follow = 1.0f - std::exp(-settings_.followSharpness * deltaTime);
  focus_ = {focus_.x + (butterflyPosition.x - focus_.x) * follow, focus_.y + (butterflyPosition.y - focus_.y) * follow,
            focus_.z + (butterflyPosition.z - focus_.z) * follow};

  Spherical s = Evaluate_(elapsed_);
  // 破片が広がっているときは、本体と破片が入る距離まで引く(上限つき)
  s.distance += std::min(std::max(spread, 0.0f) * settings_.spreadDistanceScale, settings_.maxSpreadDistance);
  const float horizontal = std::cos(s.elevation) * s.distance;
  pose_.eye = {focus_.x + std::sin(s.azimuth) * horizontal, focus_.y + std::sin(s.elevation) * s.distance,
               focus_.z + std::cos(s.azimuth) * horizontal};
  pose_.target = focus_;
  pose_.fovScale = s.fovScale;
}
