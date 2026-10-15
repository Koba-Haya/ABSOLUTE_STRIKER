#include "CloudSeaDebugUi.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <cstdio>
#endif

namespace CloudSeaDebugUi {

#ifdef USE_IMGUI
namespace {
// スライダーの範囲(雲海チューナーと同じ)
constexpr float kThicknessMax = 30.0f;
constexpr float kBumpMax = 20.0f;
constexpr float kScaleMin = 0.005f;
constexpr float kScaleMax = 0.1f;
constexpr float kThresholdMin = 0.15f;
constexpr float kThresholdMax = 0.8f;
constexpr float kSoftnessMin = 0.02f;
constexpr float kSoftnessMax = 0.5f;
constexpr float kSwellHeightMax = 30.0f;
constexpr float kShadowLengthMin = 5.0f;
constexpr float kShadowLengthMax = 80.0f;
constexpr float kDensityMin = 0.02f;
constexpr float kDensityMax = 1.0f;
constexpr float kFogMax = 0.02f;
constexpr float kSunAzimuthRange = 180.0f;
constexpr float kSunElevationMin = 5.0f;
constexpr float kSunElevationMax = 80.0f;
constexpr float kSeaHeightMin = -200.0f;
constexpr float kSeaHeightMax = 50.0f;
constexpr int kStepsMin = 3;
constexpr int kStepsMax = 32; // シェーダーの kCloudSeaMaxSteps と同じ

constexpr size_t kClipboardBufferSize = 1280;
} // namespace
#endif

bool Draw(CloudSeaSettings &settings) {
#ifdef USE_IMGUI
  bool changed = false;

  ImGui::SeparatorText("Shape");
  changed |= ImGui::SliderFloat("Thickness", &settings.thickness, 0.0f, kThicknessMax, "%.1f");
  changed |= ImGui::SliderFloat("Bump", &settings.bump, 0.0f, kBumpMax, "%.1f");
  changed |= ImGui::SliderFloat("Scale", &settings.scale, kScaleMin, kScaleMax, "%.4f");
  changed |= ImGui::SliderFloat("Threshold", &settings.threshold, kThresholdMin, kThresholdMax, "%.2f");
  changed |= ImGui::SliderFloat("Softness", &settings.softness, kSoftnessMin, kSoftnessMax, "%.2f");

  ImGui::SeparatorText("Swell / Billow");
  changed |= ImGui::SliderFloat("Swell Height", &settings.swellHeight, 0.0f, kSwellHeightMax, "%.1f");
  changed |= ImGui::SliderFloat("Swell Blend", &settings.swellBlend, 0.0f, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat("Billow", &settings.billow, 0.0f, 1.0f, "%.2f");

  ImGui::SeparatorText("Shading");
  changed |= ImGui::SliderFloat("Shadow Strength", &settings.shadowStrength, 0.0f, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat("Shadow Length", &settings.shadowLength, kShadowLengthMin, kShadowLengthMax, "%.0f");
  changed |= ImGui::SliderFloat("Gap Darkness", &settings.gapDarkness, 0.0f, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat("Aerial", &settings.aerial, 0.0f, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat("Contrast", &settings.contrast, 0.0f, 1.0f, "%.2f");

  ImGui::SeparatorText("Look");
  changed |= ImGui::SliderFloat("Density", &settings.density, kDensityMin, kDensityMax, "%.2f");
  changed |= ImGui::SliderFloat("Fog", &settings.fog, 0.0f, kFogMax, "%.4f");
  changed |= ImGui::SliderFloat("Sun Azimuth", &settings.sunAzimuthDeg, -kSunAzimuthRange, kSunAzimuthRange, "%.0f");
  changed |= ImGui::SliderFloat("Sun Elevation", &settings.sunElevationDeg, kSunElevationMin, kSunElevationMax, "%.0f");

  ImGui::SeparatorText("Placement / Cost");
  changed |= ImGui::SliderFloat("Sea Height", &settings.seaHeight, kSeaHeightMin, kSeaHeightMax, "%.1f");
  changed |= ImGui::SliderInt("Steps", &settings.steps, kStepsMin, kStepsMax);
  changed |= ImGui::Checkbox("Jitter", &settings.jitter);

  if (ImGui::Button("Reset")) {
    settings = CloudSeaSettings{};
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy values")) {
    char text[kClipboardBufferSize];
    std::snprintf(text, sizeof(text),
                  "float thickness = %.1ff;\nfloat bump = %.1ff;\nfloat scale = %.4ff;\nfloat threshold = %.2ff;\n"
                  "float softness = %.2ff;\nfloat swellHeight = %.1ff;\nfloat swellBlend = %.2ff;\nfloat billow = %.2ff;\n"
                  "float shadowStrength = %.2ff;\nfloat shadowLength = %.0ff;\nfloat gapDarkness = %.2ff;\n"
                  "float aerial = %.2ff;\nfloat contrast = %.2ff;\nfloat density = %.2ff;\nfloat fog = %.4ff;\n"
                  "float sunAzimuthDeg = %.1ff;\nfloat sunElevationDeg = %.1ff;\nfloat seaHeight = %.1ff;\n"
                  "int32_t steps = %d;\nbool jitter = %s;\n",
                  settings.thickness, settings.bump, settings.scale, settings.threshold, settings.softness,
                  settings.swellHeight, settings.swellBlend, settings.billow, settings.shadowStrength,
                  settings.shadowLength, settings.gapDarkness, settings.aerial, settings.contrast, settings.density,
                  settings.fog, settings.sunAzimuthDeg, settings.sunElevationDeg, settings.seaHeight, settings.steps,
                  settings.jitter ? "true" : "false");
    ImGui::SetClipboardText(text);
  }
  return changed;
#else
  (void)settings;
  return false;
#endif
}

} // namespace CloudSeaDebugUi
