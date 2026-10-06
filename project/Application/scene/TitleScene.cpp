#include "TitleScene.h"
#include "Input.h"
#include "Renderer.h"
#include "Method.h"
#include "GameCamera.h"
#include "AbsoluteEngine/resources/AssetManager.h"

#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "SceneIds.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kDeltaTime = 1.0f / 60.0f;

// カメラの投影(位置・向き・画角は TitleDirector が決める)
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f;

constexpr float kLogoFadeInSec = 2.0f;

// スタート演出(加速 → 白フラッシュ → ゲームへ)
constexpr float kLaunchDurationSec = 1.6f;
constexpr float kFlashStartRatio = 0.55f; // 演出のこの割合から白くなり始め、終わりで真っ白

// PRESS START の明滅(ゆっくり。完全には消えない)
constexpr float kTwoPi = 6.2831853f;
constexpr float kStartBlinkPeriodSec = 3.0f;
constexpr float kStartBlinkMinAlpha = 0.25f;

// 太陽の方向(太陽へ向かう向き)。空の描画で使う
const Vector3 kSunDirection{0.30f, 0.22f, 1.0f};

// ライト(光が進む向き)。タイトルにはライトオブジェクトが無いので直接設定する
// 主光: 昼の太陽(上から)。補助光: 機体の後ろ側が真っ黒にならないよう、後方から空色で当てる
const Vector3 kSunLightDirection{-0.3f, -0.85f, -0.35f};
const Vector3 kSunLightColor{1.0f, 0.95f, 0.85f};
constexpr float kSunLightIntensity = 1.2f;
const Vector3 kFillLightDirection{0.1f, -0.4f, 0.9f};
const Vector3 kFillLightColor{0.65f, 0.78f, 1.0f};
constexpr float kFillLightIntensity = 0.8f;

constexpr const char* kPlayerPath = "resources/app/player/dragon/dragon.obj";
constexpr const char* kLogoPath = "resources/app/title/logo.obj";
constexpr const char* kStartPath = "resources/app/title/start.obj";
constexpr const char* kFlashPath = "resources/app/title/flash.obj";

bool InitModel(ModelInstance &instance, const char *path, int32_t lightingMode) {
  auto res = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(path);
  if (!res) return false;
  ModelInstance::CreateInfo ci{};
  ci.resource = res;
  ci.lightingMode = lightingMode;
  return instance.Initialize(ci);
}
} // namespace

float TitleScene::CalcBlinkAlpha(float time) {
  // sin を 0〜1 に直し、kStartBlinkMinAlpha〜1 の範囲へ線形補間する
  const float angularSpeed = kTwoPi / kStartBlinkPeriodSec;
  const float wave01 = 0.5f + 0.5f * std::sin(time * angularSpeed);
  return kStartBlinkMinAlpha + (1.0f - kStartBlinkMinAlpha) * wave01;
}

float TitleScene::CalcLogoAlpha(float time) {
  return (std::min)(time / kLogoFadeInSec, 1.0f);
}

float TitleScene::CalcFlashAlpha(float launchU) {
  // kFlashStartRatio 〜 1 を 0〜1 に直し、smoothstep でなめらかに白くする
  const float t = (std::min)((std::max)((launchU - kFlashStartRatio) / (1.0f - kFlashStartRatio), 0.0f), 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

void TitleScene::RequestStart() {
  if (startRequested_) return;
  startRequested_ = true;
  launchTime_ = 0.0f;
}

void TitleScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  startRequested_ = false;
  leaving_ = false;
  launchTime_ = 0.0f;
  flashAlpha_ = 0.0f;
  time_ = 0.0f;
  Renderer::GetInstance()->InitializePostProcess(1280, 720);

  auto camera = std::make_shared<GameCamera>();
  camera->Initialize();
  const TitleState initial = director_.Evaluate(0.0f);
  camera->SetPerspective(initial.fovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  camera->SetEye(initial.cameraEye);
  camera->SetTarget(initial.cameraTarget);
  SetMainCamera(camera);

  InitModel(playerModel_, kPlayerPath, 1);
  // ロゴ類はライティングしない
  InitModel(logoModel_, kLogoPath, 0);
  InitModel(startModel_, kStartPath, 0);
  InitModel(flashModel_, kFlashPath, 0);
}

void TitleScene::Finalize() {}

void TitleScene::Update() {
  time_ += kDeltaTime;

  if (startRequested_) {
    launchTime_ += kDeltaTime;
  }
  const float launchU = startRequested_ ? (std::min)(launchTime_ / kLaunchDurationSec, 1.0f) : -1.0f;
  flashAlpha_ = startRequested_ ? CalcFlashAlpha(launchU) : 0.0f;
  // 演出が終わった(画面が真っ白になった)フレームで、一度だけゲームへ遷移する
  if (launchU >= 1.0f && !leaving_) {
    leaving_ = true;
    RequestSceneChange(SceneId::Game);
  }

  const TitleState state = director_.Evaluate(time_, launchU);
  GetMainCamera()->SetPerspective(state.fovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  GetMainCamera()->SetEye(state.cameraEye);
  GetMainCamera()->SetTarget(state.cameraTarget);

  playerModel_.SetWorld(state.playerWorld);

  logoModel_.SetWorld(state.logoWorld);
  logoModel_.SetColor({1.0f, 1.0f, 1.0f, CalcLogoAlpha(time_)});

  startModel_.SetWorld(state.startWorld);
  startModel_.SetColor({1.0f, 1.0f, 1.0f, CalcBlinkAlpha(time_)});

  flashModel_.SetWorld(state.flashWorld);
  flashModel_.SetColor({1.0f, 1.0f, 1.0f, flashAlpha_});

  if (services_.input) {
    if (services_.input->TriggerKey(DIK_SPACE) || services_.input->TriggerKey(DIK_RETURN) || services_.input->WasPadPressed(XINPUT_GAMEPAD_A)) {
      RequestStart();
    }
  }

#ifdef USE_IMGUI
  // 開発ビルド用の操作説明(タイトル文字とスタート案内は3Dモデル側で表示するので出さない)
  ImGui::SetNextWindowPos(ImVec2(16, 700), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
  ImGui::Begin("Title", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.6f, 1.0f), "操作方法");
  ImGui::BulletText("WASD / 矢印キー : 移動");
  ImGui::BulletText("Shift            : 回避ロール（発動中は無敵）");
  ImGui::BulletText("SPACE / 左クリック : 長押しでロックオン、離すと発射");

  ImGui::Spacing();
  if (ImGui::Button("Start", ImVec2(160, 40))) {
    RequestStart();
  }

  ImGui::End();
#endif
}

void TitleScene::ApplyTitleLights(Renderer *renderer) {
  DirLight sun;
  sun.color = kSunLightColor;
  sun.direction = Normalize(kSunLightDirection);
  sun.intensity = kSunLightIntensity;
  sun.enabled = true;
  DirLight fill;
  fill.color = kFillLightColor;
  fill.direction = Normalize(kFillLightDirection);
  fill.intensity = kFillLightIntensity;
  fill.enabled = true;
  renderer->SetDirectionalLights({sun, fill}, true);
  renderer->SetPointLights({}, true);
  renderer->SetSpotLights({}, true);
}

void TitleScene::Draw() {
  auto* renderer = Renderer::GetInstance();
  renderer->BeginRenderScene();
  renderer->SetCamera(*GetMainCamera());
  ApplyTitleLights(renderer);

  // 空は最奥に描くので順番は自由だが、不透明物の前に描いて読みやすくする
  renderer->DrawSky(Normalize(kSunDirection));
  renderer->DrawWater(Normalize(kSunDirection), time_);

  renderer->DrawModel(&playerModel_);
  // 半透明(depthWrite なし)のロゴ類は不透明物の後に描く。カメラのすぐ前に置いてあるので自機より常に手前
  renderer->DrawEffectModel(&logoModel_);
  renderer->DrawEffectModel(&startModel_);
  // 白フラッシュは一番最後(一番手前)に描く
  if (flashAlpha_ > 0.0f) {
    renderer->DrawEffectModel(&flashModel_);
  }

  renderer->EndRenderScene(Inverse(GetMainCamera()->GetProjectionMatrix()));
}
