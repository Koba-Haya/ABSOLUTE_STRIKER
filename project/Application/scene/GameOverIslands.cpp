#include "GameOverIslands.h"

#include "AbsoluteEngine/resources/AssetManager.h"
#include "Method.h"
#include "ModelResource.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 島のモデル(共有パレット island_palette.png を同じフォルダから読む)。種類は順番に割り当てる
constexpr const char *kIslandModelPaths[] = {
    "resources/app/island/island_small.gltf",
    "resources/app/island/island_medium.gltf",
    "resources/app/island/island_forest.gltf",
    "resources/app/island/island_rock.gltf",
};
// 巣の島(別のモデル。くぼみの中心が原点)
constexpr const char *kNestModelPath = "resources/app/island/island_nest.gltf";
constexpr int32_t kIslandKindCount = static_cast<int32_t>(sizeof(kIslandModelPaths) / sizeof(kIslandModelPaths[0]));

constexpr float kDegToRad = 3.14159265f / 180.0f;
constexpr float kTwoPi = 6.2831853f;
constexpr float kMaxTiltRad = 0.12f;       // 島の傾きの最大(ラジアン。約 7 度)
constexpr float kBobSpeedVariation = 0.3f; // 揺れの速さの島ごとの差(1 +- この割合)
constexpr float kStratumJitterMin = 0.1f;  // 距離の区間の中でのずらしの範囲(0〜1。区間の端を避けて、島どうしを離す)
constexpr float kStratumJitterRange = 0.8f;
constexpr float kAzimuthMinRatio = 0.15f;  // 左右の広がりのうち、正面に近すぎない最小の割合(正面の島はカメラの視線をふさぐ)
constexpr float kHalfChance = 0.5f;        // 回る向きを決める確率
constexpr size_t kLightGroupSize = 1;      // 島に当てる光は太陽の 1 つだけ
constexpr int32_t kHalfLambert = 2;        // 光の当たらない側も真っ黒にしない照明モード(ModelLightingMode::kHalfLambert)

#ifdef USE_IMGUI
constexpr float kDebugWindowX = 10.0f;
constexpr float kDebugWindowY = 400.0f;
constexpr float kDistanceSliderMax = 300.0f;
constexpr float kSpreadSliderMax = 90.0f;
constexpr float kScaleSliderMin = 0.2f;
constexpr float kScaleSliderMax = 6.0f;
constexpr int32_t kSeedSliderMax = 99999;
constexpr float kBobAmplitudeMax = 3.0f;
constexpr float kBobSpeedMax = 2.0f;
constexpr float kSpinSpeedMax = 0.5f;
constexpr float kLightMax = 3.0f;
#endif

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// 水平(x, z)の点 p と、線分 a-b の距離。島がカメラの通り道をふさがないかの判定に使う
float DistanceToSegment2D(float px, float pz, float ax, float az, float bx, float bz) {
  const float abx = bx - ax;
  const float abz = bz - az;
  const float lengthSq = abx * abx + abz * abz;
  float t = lengthSq > 0.0f ? ((px - ax) * abx + (pz - az) * abz) / lengthSq : 0.0f;
  t = std::clamp(t, 0.0f, 1.0f);
  const float dx = px - (ax + abx * t);
  const float dz = pz - (az + abz * t);
  return std::sqrt(dx * dx + dz * dz);
}
} // namespace

void GameOverIslands::Initialize(float layerBottom, float layerTop) {
  layerBottom_ = layerBottom;
  layerTop_ = layerTop;
  time_ = 0.0f;

  // モデルは 4 種を一度だけ読み込んで共有し(AssetManager のキャッシュ)、島ごとに ModelInstance を作る
  std::vector<std::shared_ptr<ModelResource>> resources;
  for (const char *path : kIslandModelPaths) {
    resources.push_back(AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(path));
  }
  loaded_ = true;
  for (int32_t i = 0; i < GameOverIslandDefaults::kMaxCount; ++i) {
    ModelInstance::CreateInfo info{};
    info.resource = resources[i % kIslandKindCount];
    info.lightingMode = kHalfLambert;
    info.specularColor = {0.0f, 0.0f, 0.0f}; // 岩と草は、つやなし
    if (!info.resource || !islands_[i].model.Initialize(info)) {
      loaded_ = false;
    }
  }
  // 巣の島。読み込めなくても、ほかの島は描ける(巣だけ描かない)
  auto nestResource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(kNestModelPath);
  ModelInstance::CreateInfo nestInfo{};
  nestInfo.resource = nestResource;
  nestInfo.lightingMode = kHalfLambert;
  nestInfo.specularColor = {0.0f, 0.0f, 0.0f};
  nestLoaded_ = nestResource && nestModel_.Initialize(nestInfo);

  Layout_();
}

void GameOverIslands::ApplyNestWorld_() {
  // 巣は正面(+z)に固定する。揺らさない(灰の集まる中心が動かないようにするため)
  nestPosition_ = {0.0f, layerBottom_ + (layerTop_ - layerBottom_) * nestLayerRatio_, nestDistance_};
  const Vector3 scale{nestScale_, nestScale_, nestScale_};
  const Vector3 rotate{0.0f, 0.0f, 0.0f};
  nestModel_.SetWorld(MakeAffineMatrix(scale, rotate, nestPosition_));
}

void GameOverIslands::Layout_() {
  ApplyNestWorld_();
  std::mt19937 rng(static_cast<uint32_t>(seed_));
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const float layerThickness = layerTop_ - layerBottom_;
  const int32_t count = (std::max)(count_, 1);

  for (int32_t i = 0; i < GameOverIslandDefaults::kMaxCount; ++i) {
    Island &island = islands_[i];

    // 距離は、範囲を島の数で区切った区間の中から 1 つずつ選ぶ(近い島と遠い島が偏らない)。
    // 左右は交互にして、正面に近すぎない範囲でばらす
    const float stratum = (static_cast<float>(i % count) + kStratumJitterMin + kStratumJitterRange * unit(rng)) / static_cast<float>(count);
    const float distance = Lerp(minDistance_, maxDistance_, stratum);
    const float side = (i % 2 == 0) ? 1.0f : -1.0f;
    const float ratio = Lerp(minLayerRatio_, maxLayerRatio_, unit(rng));
    island.scale = Lerp(scaleMin_, scaleMax_, unit(rng));

    // 巣の近くと、カメラの通り道(原点から巣まで)をふさぐ位置は、角度を引き直す(回数に上限。固定の乱数なので結果は毎回同じ)
    float x = 0.0f;
    float z = 0.0f;
    for (int32_t attempt = 0; attempt <= GameOverIslandDefaults::kAvoidRetries; ++attempt) {
      const float azimuth = side * spreadDeg_ * kDegToRad * Lerp(kAzimuthMinRatio, 1.0f, unit(rng));
      x = std::sin(azimuth) * distance;
      z = std::cos(azimuth) * distance;
      const float margin = island.scale * GameOverIslandDefaults::kClearPerScale;
      const float toNestX = x - nestPosition_.x;
      const float toNestZ = z - nestPosition_.z;
      const bool nearNest = std::sqrt(toNestX * toNestX + toNestZ * toNestZ) < nestClearRadius_ + margin;
      const bool onPath = DistanceToSegment2D(x, z, 0.0f, 0.0f, nestPosition_.x, nestPosition_.z) < pathClearWidth_ + margin;
      if (!nearNest && !onPath) break;
    }
    island.position = {x, layerBottom_ + layerThickness * ratio, z};
    island.yaw = unit(rng) * kTwoPi;
    island.tiltX = (unit(rng) * 2.0f - 1.0f) * kMaxTiltRad;
    island.tiltZ = (unit(rng) * 2.0f - 1.0f) * kMaxTiltRad;
    island.bobPhase = unit(rng) * kTwoPi;
    island.bobSpeedRatio = 1.0f + (unit(rng) * 2.0f - 1.0f) * kBobSpeedVariation;
    island.spinSign = unit(rng) < kHalfChance ? 1.0f : -1.0f;
  }
  ApplyWorlds_();
}

void GameOverIslands::ApplyWorlds_() {
  for (Island &island : islands_) {
    const float bob = std::sin(time_ * bobSpeed_ * island.bobSpeedRatio + island.bobPhase) * bobAmplitude_;
    const float yaw = island.yaw + island.spinSign * yawSpinSpeed_ * time_;
    const Vector3 position{island.position.x, island.position.y + bob, island.position.z};
    const Vector3 scale{island.scale, island.scale, island.scale};
    const Vector3 rotate{island.tiltX, yaw, island.tiltZ}; // Quaternion 版と区別するため、オイラー角を Vector3 で渡す
    island.model.SetWorld(MakeAffineMatrix(scale, rotate, position));
  }
}

void GameOverIslands::Update(float deltaTime) {
  time_ += deltaTime;
  ApplyWorlds_();
}

void GameOverIslands::Draw(const Vector3 &sunDirection) {
  if (!loaded_) return;
  auto *renderer = Renderer::GetInstance();

  // 太陽は雲の光の向きに合わせる。DirLight の direction は光が進む向きなので、太陽へ向かう向きの逆
  DirLight sun;
  sun.color = {1.0f, 1.0f, 1.0f};
  sun.direction = {-sunDirection.x, -sunDirection.y, -sunDirection.z};
  sun.intensity = lightIntensity_;
  sun.enabled = true;
  renderer->SetDirectionalLights(std::vector<DirLight>(kLightGroupSize, sun), true);

  const int32_t count = std::clamp(count_, 0, GameOverIslandDefaults::kMaxCount);
  for (int32_t i = 0; i < count; ++i) {
    renderer->DrawModel(&islands_[i].model);
  }
  if (nestLoaded_) {
    renderer->DrawModel(&nestModel_);
  }
}

#ifdef USE_IMGUI
void GameOverIslands::DrawDebugUi() {
  ImGui::SetNextWindowPos(ImVec2(kDebugWindowX, kDebugWindowY), ImGuiCond_FirstUseEver);
  ImGui::Begin("Game Over Islands");
  // 配置に関わる値が変わったら、配置を決め直す(距離の区間は島の数で決まるので、数も含む)
  bool relayout = false;
  ImGui::Text("Models loaded: %s", loaded_ ? "yes" : "NO (check resources/app/island)");
  relayout |= ImGui::SliderInt("Count", &count_, 0, GameOverIslandDefaults::kMaxCount);
  relayout |= ImGui::SliderInt("Seed", &seed_, 0, kSeedSliderMax);
  relayout |= ImGui::SliderFloat("Min distance", &minDistance_, 1.0f, kDistanceSliderMax, "%.0f");
  relayout |= ImGui::SliderFloat("Max distance", &maxDistance_, 1.0f, kDistanceSliderMax, "%.0f");
  relayout |= ImGui::SliderFloat("Spread (deg)", &spreadDeg_, 0.0f, kSpreadSliderMax, "%.0f");
  relayout |= ImGui::SliderFloat("Height min (layer ratio)", &minLayerRatio_, 0.0f, 1.0f, "%.2f");
  relayout |= ImGui::SliderFloat("Height max (layer ratio)", &maxLayerRatio_, 0.0f, 1.0f, "%.2f");
  relayout |= ImGui::SliderFloat("Scale min", &scaleMin_, kScaleSliderMin, kScaleSliderMax, "%.2f");
  relayout |= ImGui::SliderFloat("Scale max", &scaleMax_, kScaleSliderMin, kScaleSliderMax, "%.2f");
  ImGui::SeparatorText("Nest island (fixed; the camera descends to it)");
  ImGui::Text("Nest loaded: %s", nestLoaded_ ? "yes" : "NO (island_nest.gltf)");
  relayout |= ImGui::SliderFloat("Nest distance (+z)", &nestDistance_, 1.0f, kDistanceSliderMax, "%.0f");
  relayout |= ImGui::SliderFloat("Nest height (layer ratio)", &nestLayerRatio_, 0.0f, 1.0f, "%.2f");
  relayout |= ImGui::SliderFloat("Nest scale", &nestScale_, kScaleSliderMin, kScaleSliderMax, "%.2f");
  relayout |= ImGui::SliderFloat("Clear radius around nest", &nestClearRadius_, 0.0f, kDistanceSliderMax, "%.0f");
  relayout |= ImGui::SliderFloat("Clear width along camera path", &pathClearWidth_, 0.0f, kDistanceSliderMax, "%.0f");
  ImGui::SeparatorText("Motion / light");
  ImGui::SliderFloat("Bob amplitude", &bobAmplitude_, 0.0f, kBobAmplitudeMax, "%.2f");
  ImGui::SliderFloat("Bob speed", &bobSpeed_, 0.0f, kBobSpeedMax, "%.2f");
  ImGui::SliderFloat("Spin speed", &yawSpinSpeed_, 0.0f, kSpinSpeedMax, "%.3f");
  ImGui::SliderFloat("Light intensity", &lightIntensity_, 0.0f, kLightMax, "%.2f");
  if (relayout) {
    Layout_();
  }
  ImGui::End();
}
#endif
