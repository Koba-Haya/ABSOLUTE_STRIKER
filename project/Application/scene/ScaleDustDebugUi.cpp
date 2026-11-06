#include "ScaleDustDebugUi.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace ScaleDustDebugUi {

#ifdef USE_IMGUI
namespace {
// スライダーの範囲
constexpr float kLifeMax = 8.0f;
constexpr float kSizeMax = 0.6f;
constexpr float kSpeedMax = 6.0f;
constexpr float kAmountMax = 6.0f;
constexpr float kGravityRange = 6.0f;
constexpr float kWindRange = 4.0f;
constexpr float kDragMax = 4.0f;
constexpr float kTurbulenceStrengthMax = 4.0f;
constexpr float kTurbulenceFrequencyMax = 3.0f;
constexpr float kTurbulenceSpeedMax = 4.0f;
constexpr float kInheritMax = 1.5f;
constexpr int kShardBurstMaxLimit = 200;
constexpr float kShardBurstPerTriangleMax = 1.0f;
constexpr float kShardRadiusMax = 2.0f;
constexpr float kShardKickMax = 6.0f;
constexpr float kEdgeRateMax = 300.0f;
constexpr float kBodyRadiusMax = 5.0f;
} // namespace
#endif

void Draw(ScaleDustSettings &settings) {
#ifdef USE_IMGUI
  ImGui::SeparatorText("Amount");
  ImGui::SliderFloat("Amount scale", &settings.amountScale, 0.0f, kAmountMax);
  ImGui::SeparatorText("Look");
  ImGui::ColorEdit3("Ember color (birth)", &settings.emberColor.x);
  ImGui::ColorEdit3("Ash color (cooled)", &settings.ashColor.x);
  ImGui::SliderFloat("Cool ratio (of life)", &settings.coolRatio, 0.05f, 1.0f);
  ImGui::SliderFloat("Fade power", &settings.fadePower, 0.5f, 6.0f);
  ImGui::SliderFloat("Life min (s)", &settings.lifeMin, 0.1f, kLifeMax);
  ImGui::SliderFloat("Life max (s)", &settings.lifeMax, 0.1f, kLifeMax);
  ImGui::SliderFloat("Size min", &settings.sizeMin, 0.005f, kSizeMax);
  ImGui::SliderFloat("Size max", &settings.sizeMax, 0.005f, kSizeMax);
  ImGui::SliderFloat("Speed min", &settings.speedMin, 0.0f, kSpeedMax);
  ImGui::SliderFloat("Speed max", &settings.speedMax, 0.0f, kSpeedMax);
  ImGui::SeparatorText("Motion");
  ImGui::SliderFloat("Gravity", &settings.gravity, -kGravityRange, kGravityRange);
  ImGui::SliderFloat3("Wind", &settings.wind.x, -kWindRange, kWindRange);
  ImGui::SliderFloat("Drag (1/s)", &settings.dragPerSecond, 0.0f, kDragMax);
  ImGui::SliderFloat("Turbulence strength", &settings.turbulenceStrength, 0.0f, kTurbulenceStrengthMax);
  ImGui::SliderFloat("Turbulence frequency", &settings.turbulenceFrequency, 0.05f, kTurbulenceFrequencyMax);
  ImGui::SliderFloat("Turbulence speed", &settings.turbulenceSpeed, 0.0f, kTurbulenceSpeedMax);
  ImGui::SliderFloat("Inherit source velocity", &settings.inheritVelocityScale, 0.0f, kInheritMax);
  if (ImGui::Button("Reset scale dust settings")) {
    settings = ScaleDustSettings{};
  }
#else
  (void)settings;
#endif
}

void Draw(PlayerDeathDust &deathDust) {
#ifdef USE_IMGUI
  PlayerDeathDustSettings &death = deathDust.GetSettings();
  ImGui::SeparatorText("Sources (death)");
  int shardBase = static_cast<int>(death.shardBurstBase);
  int shardMax = static_cast<int>(death.shardBurstMax);
  ImGui::SliderInt("Shard burst base", &shardBase, 0, kShardBurstMaxLimit);
  ImGui::SliderFloat("Shard burst per triangle", &death.shardBurstPerTriangle, 0.0f, kShardBurstPerTriangleMax);
  ImGui::SliderInt("Shard burst max", &shardMax, 0, kShardBurstMaxLimit);
  death.shardBurstBase = static_cast<uint32_t>(shardBase);
  death.shardBurstMax = static_cast<uint32_t>(shardMax);
  ImGui::SliderFloat("Shard burst radius", &death.shardBurstRadius, 0.0f, kShardRadiusMax);
  ImGui::SliderFloat("Shard kick speed", &death.shardKickSpeed, 0.0f, kShardKickMax);
  ImGui::SliderFloat("Edge rate peak (/s)", &death.edgeRatePeak, 0.0f, kEdgeRateMax);
  ImGui::SliderFloat("Body radius", &death.bodyRadius, 0.1f, kBodyRadiusMax);
  ImGui::SliderFloat("Edge shrink", &death.edgeShrink, 0.0f, 1.0f);
  if (ImGui::Button("Reset death dust sources")) {
    death = PlayerDeathDustSettings{};
  }
  Draw(deathDust.GetDust().GetSettings());
#else
  (void)deathDust;
#endif
}

} // namespace ScaleDustDebugUi
