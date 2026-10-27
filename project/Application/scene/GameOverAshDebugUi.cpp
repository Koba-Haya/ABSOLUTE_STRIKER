#include "GameOverAshDebugUi.h"

#include "ScaleDustDebugUi.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace GameOverAshDebugUi {

#ifdef USE_IMGUI
namespace {
// 調整窓の位置(ほかの調整窓と重ならないよう、右側)
constexpr float kWindowX = 700.0f;
constexpr float kWindowY = 10.0f;
// スライダーの範囲
constexpr float kSecondsMax = 15.0f;
constexpr float kDistanceMax = 80.0f;
constexpr float kRateMax = 400.0f;
constexpr float kStiffnessMax = 4.0f;
constexpr float kDragMax = 6.0f;
constexpr float kSwirlMax = 6.0f;
constexpr float kIntensityMax = 3.0f;
constexpr float kCoreSizeMax = 4.0f;
constexpr float kCoreLiftMax = 2.0f;
constexpr float kPulseSpeedMax = 10.0f;
constexpr float kLightRadiusMax = 50.0f;
constexpr float kLightIntensityMax = 10.0f;
constexpr float kSpeedScaleMax = 8.0f;
constexpr float kGlowIntensityMax = 4.0f;
constexpr float kStreakLengthMax = 2.0f;
constexpr float kStreakMaxMax = 20.0f;
constexpr float kTwinkleSpeedMax = 30.0f;
constexpr float kSharpnessMax = 120.0f;
constexpr float kBoostMax = 3.0f;
constexpr uint32_t kReleaseCount = 60;
constexpr float kReleaseSpeed = 3.0f;

const char *PhaseName(GameOverAshPhase phase) {
  switch (phase) {
  case GameOverAshPhase::Drifting:
    return "Drifting";
  case GameOverAshPhase::Gathering:
    return "Gathering";
  case GameOverAshPhase::Glowing:
    return "Glowing";
  }
  return "?";
}
} // namespace
#endif

void Draw(GameOverAsh &ash) {
#ifdef USE_IMGUI
  GameOverAshSettings &s = ash.GetSettings();
  ImGui::SetNextWindowPos(ImVec2(kWindowX, kWindowY), ImGuiCond_FirstUseEver);
  ImGui::Begin("Game Over Ash");

  ImGui::SeparatorText("State");
  ImGui::Text("Phase: %s  Gather progress: %.2f", PhaseName(ash.GetPhase()), ash.GetGatherProgress());
  if (ImGui::Button("Drifting")) ash.SetPhase(GameOverAshPhase::Drifting);
  ImGui::SameLine();
  if (ImGui::Button("Gathering")) ash.SetPhase(GameOverAshPhase::Gathering);
  ImGui::SameLine();
  if (ImGui::Button("Glowing")) ash.SetPhase(GameOverAshPhase::Glowing);
  ImGui::SameLine();
  if (ImGui::Button("Auto")) ash.ClearPhaseOverride();
  // 値は ash が持つ(画面をまたいで残る static を持たない)
  float speedScale = ash.GetGatherSpeedScale();
  float boost = ash.GetCoreBoost();
  if (ImGui::SliderFloat("Gather speed scale (revival hook)", &speedScale, 0.0f, kSpeedScaleMax)) ash.SetGatherSpeedScale(speedScale);
  if (ImGui::SliderFloat("Core boost (revival hook)", &boost, 0.0f, kBoostMax)) ash.SetCoreBoost(boost);
  if (ImGui::Button("Release upward from the core")) ash.ReleaseUpward(kReleaseCount, kReleaseSpeed);

  ImGui::SeparatorText("Timeline (seconds since the descent started)");
  ImGui::SliderFloat("Gather start", &s.gatherStartSeconds, 0.0f, kSecondsMax);
  ImGui::SliderFloat("Gather length", &s.gatherSeconds, 0.5f, kSecondsMax);

  ImGui::SeparatorText("Spawn volume (ahead of the camera)");
  ImGui::SliderFloat("Ahead distance", &s.spawnAheadDistance, 0.0f, kDistanceMax);
  ImGui::SliderFloat("Radius", &s.spawnRadius, 1.0f, kDistanceMax);
  ImGui::SliderFloat("Spawn rate (/s)", &s.spawnRate, 0.0f, kRateMax);
  ImGui::SliderFloat("Glow spawn rate scale", &s.glowSpawnRateScale, 0.0f, 1.0f);

  ImGui::SeparatorText("Gathering forces");
  ImGui::SliderFloat("Converge stiffness", &s.convergeStiffness, 0.0f, kStiffnessMax);
  ImGui::SliderFloat("Gather drag", &s.gatherDrag, 0.0f, kDragMax);
  ImGui::SliderFloat("Swirl strength", &s.swirlStrength, 0.0f, kSwirlMax);
  ImGui::SliderFloat("Glow swirl scale", &s.glowSwirlScale, 0.0f, 1.0f);

  ImGui::SeparatorText("Ember core");
  ImGui::SliderFloat("Core intensity (settled)", &s.coreIntensity, 0.0f, kIntensityMax);
  ImGui::SliderFloat("Core ease start", &s.coreEaseStart, 0.0f, 0.9f);
  ImGui::SliderFloat("Core size", &s.core.size, 0.1f, kCoreSizeMax);
  ImGui::SliderFloat("Core lift", &s.core.lift, -kCoreLiftMax, kCoreLiftMax);
  ImGui::SliderFloat("Core min size ratio", &s.core.minSizeRatio, 0.0f, 1.0f);
  ImGui::ColorEdit3("Core hot color", &s.core.hotColor.x);
  ImGui::ColorEdit3("Core dim color", &s.core.dimColor.x);
  ImGui::SliderFloat("Pulse speed (rad/s)", &s.core.pulseSpeed, 0.0f, kPulseSpeedMax);
  ImGui::SliderFloat("Pulse depth", &s.core.pulseDepth, 0.0f, 1.0f);
  ImGui::SliderFloat("Core particle life (s)", &s.core.particleLife, 0.04f, 0.6f);
  ImGui::Checkbox("Light the nest island (point light)", &s.core.lightEnabled);
  ImGui::SliderFloat("Light radius", &s.core.lightRadius, 1.0f, kLightRadiusMax);
  ImGui::SliderFloat("Light intensity", &s.core.lightIntensity, 0.0f, kLightIntensityMax);

  ImGui::SeparatorText("Glow pass (additive, after the clouds)");
  ImGui::SliderFloat("Glow intensity", &s.glow.intensity, 0.0f, kGlowIntensityMax);
  ImGui::SliderFloat("Streak length (per speed)", &s.glow.streakLength, 0.0f, kStreakLengthMax);
  ImGui::SliderFloat("Streak max (x size)", &s.glow.streakMax, 1.0f, kStreakMaxMax);
  ImGui::SliderFloat("Twinkle amount", &s.glow.twinkleAmount, 0.0f, 1.0f);
  ImGui::SliderFloat("Twinkle speed", &s.glow.twinkleSpeed, 0.0f, kTwinkleSpeedMax);
  ImGui::SliderFloat("Core sharpness", &s.glow.coreSharpness, 2.0f, kSharpnessMax);
  ImGui::SliderFloat("Halo sharpness", &s.glow.haloSharpness, 1.0f, kSharpnessMax);
  ImGui::SliderFloat("Outer sharpness", &s.glow.outerSharpness, 0.5f, kSharpnessMax);
  ImGui::SliderFloat("Hot radius (near the nest)", &s.hotRadius, 0.5f, kDistanceMax);
  ImGui::SliderFloat("Hot boost (settled)", &s.hotBoostMax, 0.0f, 1.0f);
  ImGui::SliderFloat("Speed heat gain", &s.glow.speedHeatGain, 0.0f, 2.0f);
  ImGui::SliderFloat("Distance falloff", &s.glow.distanceFalloff, 0.0f, 0.1f, "%.4f");
  ImGui::SliderFloat("Depth bias", &s.glow.depthBias, 0.0f, 0.01f, "%.5f");
  ImGui::ColorEdit3("Heat low (dark red)", &s.glow.colorRed.x);
  ImGui::ColorEdit3("Heat mid (orange)", &s.glow.colorOrange.x);
  ImGui::ColorEdit3("Heat high (yellow)", &s.glow.colorYellow.x);
  ImGui::ColorEdit3("Core (white-yellow)", &s.glow.colorCore.x);
  ImGui::SliderFloat("Core min heat", &s.core.minHeat, 0.0f, 1.0f);
  ImGui::SliderFloat("Core glow gain", &s.core.gainPerIntensity, 0.0f, kGlowIntensityMax);

  ImGui::SeparatorText("Ash look and drift (ScaleDust)");
  ScaleDustDebugUi::Draw(s.dust);

  ImGui::End();
#else
  (void)ash;
#endif
}

} // namespace GameOverAshDebugUi
