#include "CloudImpact.h"
#include <algorithm>
#include <cmath>

namespace {
// 0 -> 1 の滑らかな補間(エルミート)
float SmoothStep(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (std::max)(edge1 - edge0, 1.0e-4f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
// 最初が速く、だんだん遅くなる動き(膨らみ、立ち上がりに使う)。t: 0 -> 1
float EaseOutCubic(float t) {
  const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
  return 1.0f - u * u * u;
}
} // namespace

void CloudImpact::Trigger(const Vector3 &position, float strength) {
  active_ = true;
  age_ = 0.0f;
  strength_ = strength;
  centerX_ = position.x;
  centerZ_ = position.z;
  centerY_ = position.y;
}

void CloudImpact::Reset() {
  active_ = false;
  age_ = 0.0f;
  strength_ = 0.0f;
}

void CloudImpact::Update(float deltaTime) {
  if (!active_) {
    return;
  }
  age_ += deltaTime;
  if (age_ >= Lifetime_()) {
    Reset();
  }
}

float CloudImpact::Lifetime_() const {
  const float puffSeconds = puff_.appearSeconds + puff_.holdSeconds + puff_.fadeSeconds;
  return (std::max)({shape_.refillSeconds, shape_.ringFadeSeconds, puffSeconds}) + kLifetimeMarginSeconds;
}

void CloudImpact::ApplyToSea(VolumeCloudSettings &sea) const {
  // 形の調整値を写し、続いている間だけ、強さ・位置・経過秒を渡す
  sea.impact = shape_;
  sea.impact.strength = active_ ? strength_ : 0.0f;
  sea.impact.centerX = centerX_;
  sea.impact.centerZ = centerZ_;
  sea.impact.age = age_;
  // 雲海を常に満たす量(seaFill、seaFillThreshold)は、上の形の調整値の写しで、そのまま渡る(時間では変えない)
}

bool CloudImpact::BuildPuff(VolumeCloudSettings &out) const {
  if (!active_) {
    return false;
  }
  // 経過時間から、大きさ・高さ・濃さを決める
  const float grow = EaseOutCubic(age_ / (std::max)(puff_.growSeconds, 1.0e-4f));
  const float appear = SmoothStep(0.0f, puff_.appearSeconds, age_);
  const float fadeStart = puff_.appearSeconds + puff_.holdSeconds;
  const float vanish = 1.0f - SmoothStep(fadeStart, fadeStart + puff_.fadeSeconds, age_);
  const float amount = appear * vanish;
  if (amount <= 0.0f) {
    return false; // 消えたあとは、描かない(無駄な負荷を避ける)
  }

  VolumeCloudSettings box; // 箱の既定(ノイズの細かさや光の強さ)から始める
  box.enabled = true;
  box.shape = VolumeCloudShape::Box;
  box.center = {centerX_, centerY_ + puff_.riseStart + (puff_.riseEnd - puff_.riseStart) * grow, centerZ_};
  box.halfSize = {puff_.halfSizeStart.x + (puff_.halfSizeEnd.x - puff_.halfSizeStart.x) * grow,
                  puff_.halfSizeStart.y + (puff_.halfSizeEnd.y - puff_.halfSizeStart.y) * grow,
                  puff_.halfSizeStart.z + (puff_.halfSizeEnd.z - puff_.halfSizeStart.z) * grow};
  box.density = puff_.density * amount;
  box.threshold = puff_.threshold;
  box.edgeFade = puff_.edgeFade;
  out = box;
  return true;
}
