#include "ResultBackdrop.h"

#include "GameCamera.h"
#include "Method.h"
#include "Renderer.h"

namespace {
// 描画先の大きさ(タイトル・ゲームと同じにする)
constexpr uint32_t kSceneWidth = 1280;
constexpr uint32_t kSceneHeight = 720;

// カメラの投影
constexpr float kCameraFovY = 0.9f;
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f;

// 太陽の方向(太陽へ向かう向き)。タイトルと同じ空にする
const Vector3 kSunDirection{0.30f, 0.22f, 1.0f};
} // namespace

void ResultBackdrop::Initialize(float targetHeight) {
  Renderer::GetInstance()->InitializePostProcess(kSceneWidth, kSceneHeight);

  camera_ = std::make_shared<GameCamera>();
  camera_->Initialize();
  camera_->SetPerspective(kCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  camera_->SetEye({0.0f, 0.0f, 0.0f});
  camera_->SetTarget({0.0f, targetHeight, 1.0f});
  time_ = 0.0f;
}

void ResultBackdrop::Begin() {
  auto *renderer = Renderer::GetInstance();
  renderer->SetCamera(*camera_);
  renderer->BeginRenderScene();
  renderer->DrawSky(Normalize(kSunDirection), time_);
}

void ResultBackdrop::End() {
  Renderer::GetInstance()->EndRenderScene(Inverse(camera_->GetProjectionMatrix()));
}
