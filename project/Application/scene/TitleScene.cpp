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

// カメラ(固定。L0-4 で TitleDirector に置き換える)
constexpr float kCameraFovY = 0.45f;
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f;
const Vector3 kCameraEye{0.0f, 0.0f, -10.0f};
const Vector3 kCameraTarget{0.0f, 0.0f, 0.0f};

// ロゴ・PRESS START の配置(板ポリは横幅 2 が基準なので scale が半幅になる)
constexpr float kLogoHalfWidth = 3.0f;
constexpr float kLogoPosY = 1.4f;
constexpr float kStartHalfWidth = 1.5f;
constexpr float kStartPosY = -1.8f;
constexpr float kLogoFadeInSec = 2.0f;

// PRESS START の明滅(ゆっくり。完全には消えない)
constexpr float kTwoPi = 6.2831853f;
constexpr float kStartBlinkPeriodSec = 3.0f;
constexpr float kStartBlinkMinAlpha = 0.25f;

// 自機(仮モデル)
constexpr float kPlayerPosY = -0.2f;
constexpr float kPlayerPosZ = -4.0f;
constexpr float kPlayerSpinSpeed = 1.0f;

// 夕日のライト(光が進む向き。L0-2 の太陽方向と揃える)。タイトルにはライトオブジェクトが無いので直接設定する
const Vector3 kSunLightDirection{-0.4f, -0.3f, 0.8f};
const Vector3 kSunLightColor{1.0f, 0.7f, 0.45f};
constexpr float kSunLightIntensity = 1.2f;

constexpr const char* kPlayerPath = "resources/app/player/player.obj";
constexpr const char* kLogoPath = "resources/app/title/logo.obj";
constexpr const char* kStartPath = "resources/app/title/start.obj";

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

void TitleScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  startRequested_ = false;
  time_ = 0.0f;
  Renderer::GetInstance()->InitializePostProcess(1280, 720);

  auto camera = std::make_shared<GameCamera>();
  camera->Initialize();
  camera->SetPerspective(kCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  camera->SetEye(kCameraEye);
  camera->SetTarget(kCameraTarget);
  SetMainCamera(camera);

  InitModel(playerModel_, kPlayerPath, 1);
  // ロゴ類はライティングしない
  InitModel(logoModel_, kLogoPath, 0);
  InitModel(startModel_, kStartPath, 0);
}

void TitleScene::Finalize() {}

void TitleScene::Update() {
  time_ += kDeltaTime;

  playerModel_.SetWorld(MakeAffineMatrix(Vector3{1.0f, 1.0f, 1.0f}, Vector3{0.0f, time_ * kPlayerSpinSpeed, 0.0f}, Vector3{0.0f, kPlayerPosY, kPlayerPosZ}));

  logoModel_.SetWorld(MakeAffineMatrix(Vector3{kLogoHalfWidth, kLogoHalfWidth, 1.0f}, Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, kLogoPosY, 0.0f}));
  logoModel_.SetColor({1.0f, 1.0f, 1.0f, CalcLogoAlpha(time_)});

  startModel_.SetWorld(MakeAffineMatrix(Vector3{kStartHalfWidth, kStartHalfWidth, 1.0f}, Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, kStartPosY, 0.0f}));
  startModel_.SetColor({1.0f, 1.0f, 1.0f, CalcBlinkAlpha(time_)});

  if (services_.input) {
    if (services_.input->TriggerKey(DIK_SPACE) || services_.input->TriggerKey(DIK_RETURN) || services_.input->WasPadPressed(XINPUT_GAMEPAD_A)) {
      RequestSceneChange(SceneId::Game);
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
    RequestSceneChange(SceneId::Game);
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
  renderer->SetDirectionalLights({sun}, true);
  renderer->SetPointLights({}, true);
  renderer->SetSpotLights({}, true);
}

void TitleScene::Draw() {
  auto* renderer = Renderer::GetInstance();
  renderer->BeginRenderScene();
  renderer->SetCamera(*GetMainCamera());
  ApplyTitleLights(renderer);

  renderer->DrawModel(&playerModel_);
  // 半透明(depthWrite なし)のロゴ類は不透明物の後に描く
  renderer->DrawEffectModel(&logoModel_);
  renderer->DrawEffectModel(&startModel_);

  renderer->EndRenderScene(Inverse(GetMainCamera()->GetProjectionMatrix()));
}
