#include "CloudImpactDebugUi.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

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
constexpr float kRingWobbleMax = 8.0f;
constexpr float kSecondsMax = 6.0f;
constexpr float kPuffSizeMax = 20.0f;
constexpr float kPuffRiseRange = 15.0f;
constexpr float kPuffDensityMax = 0.6f;
} // namespace

void CloudImpactDebugUi::Draw(CloudImpact &impact, const Vector3 &defaultPosition) {
  // 着地点と強さは、ボタンで反応を起こすときに使う(メンバとして、呼び出しをまたいで保持)
  if (!initialized_) {
    position_ = defaultPosition;
    initialized_ = true;
  }

  ImGui::Text("Active: %s  Age: %.2f s", impact.IsActive() ? "yes" : "no", impact.GetAge());
  ImGui::SliderFloat("Landing x", &position_.x, -kPositionXRange, kPositionXRange);
  ImGui::SliderFloat("Landing z", &position_.z, kPositionZMin, kPositionZMax);
  ImGui::SliderFloat("Strength", &strength_, 0.0f, kStrengthMax);
  if (ImGui::Button("Use player position")) {
    position_ = defaultPosition;
  }
  ImGui::SameLine();
  if (ImGui::Button("Cloud Impact")) {
    impact.Trigger(position_, strength_);
  }

  // 形と塊の調整値は、コピーを編集して、最後に Set で戻す
  VolumeCloudImpact shape = impact.GetShape();
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
  ImGui::SliderFloat("Ring ease (0 = constant speed)", &shape.ringEase, 0.0f, 1.0f);
  ImGui::SliderFloat("Ring wobble", &shape.ringWobble, 0.0f, kRingWobbleMax);
  ImGui::SliderFloat("Ring break", &shape.ringBreak, 0.0f, 1.0f);
  ImGui::SeparatorText("Puff (box cloud)");
  CloudImpactPuff puff = impact.GetPuff();
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
  impact.SetShape(shape);
  impact.SetPuff(puff);
}
#else
void CloudImpactDebugUi::Draw(CloudImpact &, const Vector3 &) {}
#endif
