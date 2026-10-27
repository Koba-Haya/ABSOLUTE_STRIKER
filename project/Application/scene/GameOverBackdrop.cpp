#include "GameOverBackdrop.h"

#include "GameCamera.h"
#include "Method.h"
#include "Renderer.h"
#include "graphics/2d/Sprite.h"

namespace {
// 描画先の大きさ(タイトル・ゲームと同じにする)
constexpr uint32_t kSceneWidth = 1280;
constexpr uint32_t kSceneHeight = 720;

// カメラの投影
constexpr float kCameraFovY = 0.9f;
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f; // 雲の層の描く距離の上限(maxDistance)と同じにする

// カメラの位置と向き。雲の層の上面から少し上に浮かび、水平線よりやや下を向く(見渡す雲の海と、上に残る空)
constexpr float kEyeAboveCloudTop = 8.0f;  // 雲の層の上面からカメラまでの高さ
constexpr float kLookDownPerUnit = -0.20f; // 前方 1 ユニットに対する、注視点の高さ(負で下向き)

// 空の見た目: 太陽は、カメラの後ろ(方位 180 度)の高い所に置く。曇天なので、画面に太陽の光の輪を入れない
// (輪が画面に入ると、足し込まれた光が色ごとに飽和して、灰色の領域と虹のような縁が出る)。雲の光の向きも、この太陽に従う
constexpr float kSunAzimuthDeg = 180.0f;
constexpr float kSunElevationDeg = 30.0f;
// 雲の切れ間の下の層を暗くする(雲の切れ間が、深い底に見える)
constexpr float kGapDarkness = 0.9f;

// 雲の光を弱める倍率(曇天の薄暗さ)。ゲームの雲海の設定(MakeSeaLayerSettings)に掛ける
constexpr float kCloudSunScale = 0.55f;
constexpr float kCloudAmbientScale = 0.60f;

// 空を暗くする膜の濃さ(黒の不透明度)と、使う白画像(色は黒に塗る)
constexpr float kDimAlpha = 0.35f;
constexpr const char *kWhiteTexturePath = "resources/app/cube/white100x100.png";

// スプライトの座標は描画先のピクセル単位
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;
} // namespace

GameOverBackdrop::GameOverBackdrop() = default;
GameOverBackdrop::~GameOverBackdrop() = default;

void GameOverBackdrop::Initialize() {
  Renderer::GetInstance()->InitializePostProcess(kSceneWidth, kSceneHeight);

  // 雲の層は、ゲームの雲海と同じ既定値から作り、光だけを弱める
  cloudLayer_ = MakeSeaLayerSettings();
  cloudLayer_.sunIntensity *= kCloudSunScale;
  cloudLayer_.ambientIntensity *= kCloudAmbientScale;
  seaSky_.sunAzimuthDeg = kSunAzimuthDeg;
  seaSky_.sunElevationDeg = kSunElevationDeg;
  seaSky_.gapDarkness = kGapDarkness;

  const float eyeY = cloudLayer_.layerTop + kEyeAboveCloudTop;
  camera_ = std::make_shared<GameCamera>();
  camera_->Initialize();
  camera_->SetPerspective(kCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  camera_->SetEye({0.0f, eyeY, 0.0f});
  camera_->SetTarget({0.0f, eyeY + kLookDownPerUnit, 1.0f});

  Sprite::CreateInfo info;
  info.texturePath = kWhiteTexturePath;
  info.size = {kScreenWidth, kScreenHeight};
  info.color = {0.0f, 0.0f, 0.0f, kDimAlpha};
  dim_ = std::make_unique<Sprite>();
  dim_->Initialize(info);
  dim_->SetPosition({0.0f, 0.0f, 0.0f});
  time_ = 0.0f;
}

void GameOverBackdrop::Begin() {
  auto *renderer = Renderer::GetInstance();
  renderer->SetCamera(*camera_);
  renderer->BeginRenderScene();
  renderer->DrawSeaSky(seaSky_);
  // 雲の層は、物体を描き終えたあと(EndRenderScene)に描かれる。毎フレーム依頼する。光の向きは空と同じにする
  renderer->AddVolumeCloud(cloudLayer_, seaSky_.SunDirection(), time_);
  // 空を暗くする膜。雲はこのあとで重なるので、この膜は空だけを暗くする
  dim_->Draw();
}

void GameOverBackdrop::ResolveClouds() {
  Renderer::GetInstance()->FlushVolumeClouds();
}

void GameOverBackdrop::End() {
  Renderer::GetInstance()->EndRenderScene(Inverse(camera_->GetProjectionMatrix()));
}
