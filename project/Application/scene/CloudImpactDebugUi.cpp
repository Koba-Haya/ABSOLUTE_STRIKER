#include "CloudImpactDebugUi.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace CloudImpactDebugUi {

#ifdef USE_IMGUI
namespace {
// スライダーの範囲
constexpr float kPositionXRange = 300.0f;
constexpr float kPositionZMin = -100.0f;
constexpr float kPositionZMax = 1300.0f; // レールの終点(z = 1200)より少し先まで
constexpr float kStrengthMax = 3.0f;
constexpr float kCraterRadiusMax = 20.0f;
constexpr float kCraterDepthMax = 20.0f; // 層の厚み(20)まで
constexpr float kRingWidthMax = 10.0f;
constexpr float kRingHeightMax = 15.0f;
constexpr float kRingSpeedMax = 40.0f;
constexpr float kSecondsMax = 6.0f;
constexpr float kPuffSizeMax = 20.0f;
constexpr float kPuffRiseRange = 15.0f;
constexpr float kPuffDensityMax = 0.6f;
constexpr float kDefaultStrength = 1.0f;
} // namespace

void Draw(CloudImpact &impact, const Vector3 &defaultPosition) {
  // 着地点と強さは、ボタンで反応を起こすときに使う(呼び出しをまたいで保持)
  static Vector3 position{0.0f, 0.0f, 0.0f};
  static float strength = kDefaultStrength;
  static bool initialized = false;
  if (!initialized) {
    position = defaultPosition;
    initialized = true;
  }

  ImGui::Text("Active: %s  Age: %.2f s", impact.IsActive() ? "yes" : "no", impact.GetAge());
  ImGui::SliderFloat("Landing x", &position.x, -kPositionXRange, kPositionXRange);
  ImGui::SliderFloat("Landing z", &position.z, kPositionZMin, kPositionZMax);
  ImGui::SliderFloat("Strength", &strength, 0.0f, kStrengthMax);
  if (ImGui::Button("Use player position")) {
    position = defaultPosition;
  }
  ImGui::SameLine();
  if (ImGui::Button("Cloud Impact")) {
    impact.Trigger(position, strength);
  }

  VolumeCloudImpact &shape = impact.GetShape();
  ImGui::SeparatorText("Sea fill (always on: fewer gaps in the sea)");
  ImGui::SliderFloat("Sea fill", &shape.seaFill, 0.0f, 1.0f);
  ImGui::SliderFloat("Sea fill threshold", &shape.seaFillThreshold, 0.0f, 1.0f);
  ImGui::SliderFloat("Sea fill relief", &shape.seaFillRelief, 0.0f, 1.0f);
  ImGui::SeparatorText("Crater");
  ImGui::SliderFloat("Crater radius", &shape.craterRadius, 0.5f, kCraterRadiusMax);
  ImGui::SliderFloat("Crater depth", &shape.craterDepth, 0.0f, kCraterDepthMax);
  ImGui::SliderFloat("Refill (s)", &shape.refillSeconds, 0.1f, kSecondsMax);
  ImGui::SeparatorText("Ring");
  ImGui::SliderFloat("Ring width", &shape.ringWidth, 0.5f, kRingWidthMax);
  ImGui::SliderFloat("Ring height", &shape.ringHeight, 0.0f, kRingHeightMax);
  ImGui::SliderFloat("Ring speed", &shape.ringSpeed, 0.0f, kRingSpeedMax);
  ImGui::SliderFloat("Ring fade (s)", &shape.ringFadeSeconds, 0.1f, kSecondsMax);
  ImGui::SeparatorText("Puff (box cloud)");
  CloudImpactPuff &puff = impact.GetPuff();
  ImGui::SliderFloat3("Size start", &puff.halfSizeStart.x, 0.5f, kPuffSizeMax);
  ImGui::SliderFloat3("Size end", &puff.halfSizeEnd.x, 0.5f, kPuffSizeMax);
  ImGui::SliderFloat("Rise start", &puff.riseStart, -kPuffRiseRange, kPuffRiseRange);
  ImGui::SliderFloat("Rise end", &puff.riseEnd, -kPuffRiseRange, kPuffRiseRange);
  ImGui::SliderFloat("Grow (s)", &puff.growSeconds, 0.1f, kSecondsMax);
  ImGui::SliderFloat("Appear (s)", &puff.appearSeconds, 0.05f, kSecondsMax);
  ImGui::SliderFloat("Hold (s)", &puff.holdSeconds, 0.0f, kSecondsMax);
  ImGui::SliderFloat("Fade (s)", &puff.fadeSeconds, 0.1f, kSecondsMax);
  ImGui::SliderFloat("Puff density", &puff.density, 0.01f, kPuffDensityMax);
  ImGui::SliderFloat("Puff threshold", &puff.threshold, 0.1f, 0.95f);
  ImGui::SliderFloat("Puff edge fade", &puff.edgeFade, 0.05f, 1.0f);
  if (ImGui::Button("Reset shape")) {
    shape = VolumeCloudImpact{};
    puff = CloudImpactPuff{};
  }
}
#else
void Draw(CloudImpact &, const Vector3 &) {}
#endif

} // namespace CloudImpactDebugUi
