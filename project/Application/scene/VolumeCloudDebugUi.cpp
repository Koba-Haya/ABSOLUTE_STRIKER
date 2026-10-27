#include "VolumeCloudDebugUi.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <cstdio>
#endif

namespace VolumeCloudDebugUi {

#ifdef USE_IMGUI
namespace {
// スライダーの範囲
constexpr float kPositionXRange = 300.0f;
constexpr float kPositionYMin = -80.0f;
constexpr float kPositionYMax = 120.0f;
constexpr float kPositionZMin = -100.0f;
constexpr float kPositionZMax = 1300.0f; // レールの終点(z = 1200)より少し先まで
constexpr float kLayerHeightMin = -200.0f;
constexpr float kLayerHeightMax = 100.0f;
constexpr float kMaxDistanceMin = 100.0f;
constexpr float kMaxDistanceMax = 1000.0f; // カメラの遠方クリップ(1000)まで
constexpr float kFogDensityMax = 0.01f;
constexpr float kHalfSizeMin = 2.0f;
constexpr float kHalfSizeMax = 300.0f;
constexpr float kNoiseScaleMin = 0.003f;
constexpr float kNoiseScaleMax = 0.1f;
constexpr float kDetailScaleMin = 1.0f;
constexpr float kDetailScaleMax = 16.0f;
constexpr float kThresholdMin = 0.2f;
constexpr float kThresholdMax = 0.95f;
constexpr float kEdgeFadeMin = 0.02f;
constexpr float kWindRange = 20.0f;
constexpr float kDensityMin = 0.005f;
constexpr float kDensityMax = 0.5f;
constexpr float kSunIntensityMax = 5.0f;
constexpr float kAmbientIntensityMax = 3.0f;
constexpr float kPhaseGMax = 0.95f;
constexpr float kHighlightKneeMin = 0.3f;
constexpr int kStepsMin = 4;
constexpr int kStepsMax = 64; // シェーダーの kMaxSteps と同じ
constexpr int kLightStepsMax = 8; // シェーダーの kMaxLightSteps と同じ
constexpr float kLightStepLengthMin = 1.0f;
constexpr float kLightStepLengthMax = 30.0f;

constexpr float kMoundAmplitudeMax = 0.8f;
constexpr float kMoundScaleMin = 0.1f;
constexpr float kMoundScaleMax = 3.0f;
constexpr float kRimStrengthMax = 3.0f;
constexpr float kRimWidthMin = 0.01f;
constexpr float kRimPowerMin = 0.1f;
constexpr float kRimPowerMax = 6.0f;

constexpr size_t kClipboardBufferSize = 2048;
} // namespace
#endif

bool Draw(VolumeCloudSettings &settings) {
#ifdef USE_IMGUI
  bool changed = false;

  changed |= ImGui::Checkbox("Enabled", &settings.enabled);

  if (settings.shape == VolumeCloudShape::Layer) {
    ImGui::SeparatorText("Layer");
    changed |= ImGui::SliderFloat("Layer Bottom", &settings.layerBottom, kLayerHeightMin, kLayerHeightMax, "%.1f");
    changed |= ImGui::SliderFloat("Layer Top", &settings.layerTop, kLayerHeightMin, kLayerHeightMax, "%.1f");
    changed |= ImGui::SliderFloat("Max Distance", &settings.maxDistance, kMaxDistanceMin, kMaxDistanceMax, "%.0f");
    changed |= ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, kFogDensityMax, "%.4f");
  } else {
    ImGui::SeparatorText("Box");
    changed |= ImGui::SliderFloat("Center X", &settings.center.x, -kPositionXRange, kPositionXRange, "%.1f");
    changed |= ImGui::SliderFloat("Center Y", &settings.center.y, kPositionYMin, kPositionYMax, "%.1f");
    changed |= ImGui::SliderFloat("Center Z", &settings.center.z, kPositionZMin, kPositionZMax, "%.1f");
    changed |= ImGui::SliderFloat("Half Size X", &settings.halfSize.x, kHalfSizeMin, kHalfSizeMax, "%.1f");
    changed |= ImGui::SliderFloat("Half Size Y", &settings.halfSize.y, kHalfSizeMin, kHalfSizeMax, "%.1f");
    changed |= ImGui::SliderFloat("Half Size Z", &settings.halfSize.z, kHalfSizeMin, kHalfSizeMax, "%.1f");
  }

  ImGui::SeparatorText("Shape");
  changed |= ImGui::SliderFloat("Noise Scale", &settings.noiseScale, kNoiseScaleMin, kNoiseScaleMax, "%.4f");
  changed |= ImGui::SliderFloat("Detail Scale", &settings.detailScale, kDetailScaleMin, kDetailScaleMax, "%.1f");
  changed |= ImGui::SliderFloat("Threshold", &settings.threshold, kThresholdMin, kThresholdMax, "%.2f");
  changed |= ImGui::SliderFloat("Edge Fade", &settings.edgeFade, kEdgeFadeMin, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat("Erosion", &settings.erosion, 0.0f, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat3("Wind", &settings.wind.x, -kWindRange, kWindRange, "%.1f");

  ImGui::SeparatorText("Light");
  changed |= ImGui::SliderFloat("Density", &settings.density, kDensityMin, kDensityMax, "%.3f");
  changed |= ImGui::SliderFloat("Sun Intensity", &settings.sunIntensity, 0.0f, kSunIntensityMax, "%.2f");
  changed |= ImGui::SliderFloat("Ambient Intensity", &settings.ambientIntensity, 0.0f, kAmbientIntensityMax, "%.2f");
  changed |= ImGui::SliderFloat("Phase G", &settings.phaseG, 0.0f, kPhaseGMax, "%.2f");
  changed |= ImGui::SliderFloat("Highlight Knee", &settings.highlightKnee, kHighlightKneeMin, 1.0f, "%.2f");
  changed |= ImGui::SliderFloat("Shadow Lift", &settings.shadowLift, 0.0f, 1.0f, "%.2f");
  if (settings.shape == VolumeCloudShape::Layer) {
    ImGui::SeparatorText("Mounds (sea layer top relief)");
    changed |= ImGui::SliderFloat("Mound Amplitude", &settings.moundAmplitude, 0.0f, kMoundAmplitudeMax, "%.2f");
    changed |= ImGui::SliderFloat("Mound Scale", &settings.moundScale, kMoundScaleMin, kMoundScaleMax, "%.2f");
    ImGui::SeparatorText("Rim light (sea layer)");
    changed |= ImGui::SliderFloat("Rim Strength", &settings.rimStrength, 0.0f, kRimStrengthMax, "%.2f");
    changed |= ImGui::SliderFloat("Rim Width", &settings.rimWidth, kRimWidthMin, 1.0f, "%.3f");
    changed |= ImGui::SliderFloat("Rim Power", &settings.rimPower, kRimPowerMin, kRimPowerMax, "%.2f");
    ImGui::SeparatorText("Debug view (no lighting)");
    changed |= ImGui::RadioButton("Off", &settings.debugView, 0);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("Thickness", &settings.debugView, 1);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("Surface height", &settings.debugView, 2);
  }

  ImGui::SeparatorText("Cost");
  changed |= ImGui::SliderInt("Resolution Divisor", &settings.resolutionDivisor, kVolumeCloudMinResolutionDivisor,
                              kVolumeCloudMaxResolutionDivisor);
  changed |= ImGui::SliderInt("Steps", &settings.steps, kStepsMin, kStepsMax);
  changed |= ImGui::SliderInt("Light Steps", &settings.lightSteps, 0, kLightStepsMax);
  changed |= ImGui::SliderFloat("Light Step Length", &settings.lightStepLength, kLightStepLengthMin, kLightStepLengthMax, "%.1f");

  if (ImGui::Button("Reset")) {
    settings = settings.shape == VolumeCloudShape::Layer ? MakeSeaLayerSettings() : VolumeCloudSettings{};
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy values")) {
    char text[kClipboardBufferSize];
    std::snprintf(text, sizeof(text),
                  "float layerBottom = %.1ff;\nfloat layerTop = %.1ff;\nfloat maxDistance = %.0ff;\nfloat fogDensity = %.4ff;\n"
                  "Vector3 center{%.1ff, %.1ff, %.1ff};\nVector3 halfSize{%.1ff, %.1ff, %.1ff};\n"
                  "float noiseScale = %.4ff;\nfloat detailScale = %.1ff;\nfloat threshold = %.2ff;\n"
                  "float edgeFade = %.2ff;\nfloat erosion = %.2ff;\nVector3 wind{%.1ff, %.1ff, %.1ff};\n"
                  "float density = %.3ff;\nfloat sunIntensity = %.2ff;\nfloat ambientIntensity = %.2ff;\n"
                  "float phaseG = %.2ff;\nfloat highlightKnee = %.2ff;\nfloat shadowLift = %.2ff;\nint32_t resolutionDivisor = %d;\nint32_t steps = %d;\nint32_t lightSteps = %d;\nfloat lightStepLength = %.1ff;\n",
                  settings.layerBottom, settings.layerTop, settings.maxDistance, settings.fogDensity,
                  settings.center.x, settings.center.y, settings.center.z, settings.halfSize.x, settings.halfSize.y,
                  settings.halfSize.z, settings.noiseScale, settings.detailScale, settings.threshold, settings.edgeFade,
                  settings.erosion, settings.wind.x, settings.wind.y, settings.wind.z, settings.density,
                  settings.sunIntensity, settings.ambientIntensity, settings.phaseG, settings.highlightKnee, settings.shadowLift,
                  settings.resolutionDivisor, settings.steps,
                  settings.lightSteps,
                  settings.lightStepLength);
    ImGui::SetClipboardText(text);
  }
  return changed;
#else
  (void)settings;
  return false;
#endif
}

void DrawQuality(VolumeCloudQuality &quality, const VolumeCloudSettings &base) {
#ifdef USE_IMGUI
  VolumeCloudQualityConfig &config = quality.GetConfig();
  ImGui::SeparatorText("Auto Quality");
  ImGui::Checkbox("Auto Quality", &config.enabled);
  const VolumeCloudSettings applied = quality.Apply(base);
  ImGui::Text("Tier: %d / %d   GPU frame: %.1f ms", quality.GetTier(), VolumeCloudQuality::kTierCount - 1,
              quality.GetSmoothedMs());
  ImGui::Text("Applied: divisor %d, steps %d, light steps %d", applied.resolutionDivisor, applied.steps, applied.lightSteps);
  ImGui::SliderFloat("Down above (ms)", &config.downMs, 1.0f, 30.0f, "%.1f");
  ImGui::SliderFloat("Up below (ms)", &config.upMs, 1.0f, 30.0f, "%.1f");
#else
  (void)quality;
  (void)base;
#endif
}

} // namespace VolumeCloudDebugUi
