#include "GameOverBackdrop.h"

#include "GameCamera.h"
#include "Method.h"
#include "Renderer.h"
#include "graphics/2d/Sprite.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "VolumeCloudDebugUi.h"
#include "externals/imgui/imgui.h"
#endif

namespace {
// 描画先の大きさ(タイトル・ゲームと同じにする)
constexpr uint32_t kSceneWidth = 1280;
constexpr uint32_t kSceneHeight = 720;

// カメラの投影
constexpr float kCameraFovY = 0.9f;
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f; // 雲の層の描く距離の上限(maxDistance)と同じにする

// 度からラジアンへ
constexpr float kDegToRad = 3.14159265f / 180.0f;

// カメラが、前(+z)へ向ける 1 ユニットあたりの水平距離。注視点は、目の位置からこの距離だけ先に置く
constexpr float kLookDistance = 1.0f;
// 晴れきったと見なす進み具合(浮動小数の誤差で 1 に届かない場合に備える)
constexpr float kSettledEase = 0.999f;
// 見下ろす角度の大きさの下限(度)。0 に近いと tan が 0 になり、後ろへの距離が無限大になる
constexpr float kMinEndPitchMagnitudeDeg = 1.0f;

// 使う白画像(膜の色は、スプライトの色で塗る)
constexpr const char *kWhiteTexturePath = "resources/app/cube/white100x100.png";

// スプライトの座標は描画先のピクセル単位
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;

// 0〜1 の値を、始まりと終わりがなめらかな曲線にする
float Smooth(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

#ifdef USE_IMGUI
// 調整窓の位置(左上の Title / Retry の窓と重ならないよう、その下)と、スライダーの範囲
constexpr float kDebugWindowX = 10.0f;
constexpr float kDebugWindowY = 56.0f;
constexpr float kPitchMinDeg = -89.0f; // 真下(-90)は、カメラの上方向が定まらない
constexpr float kPitchMaxDeg = 60.0f;
constexpr float kDensityMax = 0.5f;
constexpr float kDescentSliderMax = 15.0f;
constexpr float kEndHeightMax = 40.0f;
constexpr float kScaleMax = 2.0f;
#endif
} // namespace

GameOverBackdrop::GameOverBackdrop() = default;
GameOverBackdrop::~GameOverBackdrop() = default;

void GameOverBackdrop::Initialize() {
  Renderer::GetInstance()->InitializePostProcess(kSceneWidth, kSceneHeight);

  // 雲の層・空・太陽の向きは、ゲームの雲海と同じ既定値のまま使う(落下の最後と見た目をそろえるため)
  baseCloud_ = MakeSeaLayerSettings();
  cloudLayer_ = baseCloud_;

  camera_ = std::make_shared<GameCamera>();
  camera_->Initialize();
  camera_->SetPerspective(kCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);

  Sprite::CreateInfo info;
  info.texturePath = kWhiteTexturePath;
  info.size = {kScreenWidth, kScreenHeight};
  info.color = {0.0f, 0.0f, 0.0f, 0.0f};
  skyVeil_ = std::make_unique<Sprite>();
  skyVeil_->Initialize(info);
  skyVeil_->SetPosition({0.0f, 0.0f, 0.0f});
  tint_ = std::make_unique<Sprite>();
  tint_->Initialize(info);
  tint_->SetPosition({0.0f, 0.0f, 0.0f});

  time_ = 0.0f;
  islands_.Initialize(baseCloud_.layerBottom, baseCloud_.layerTop);
  ApplyLook_(0.0f); // 最初のフレームから、切り替わった直後の状態にしておく
}

void GameOverBackdrop::Update(float deltaTime, float revealProgress) {
  time_ += deltaTime;
  islands_.Update(deltaTime);
  const float progress = debugRevealOverride_ ? debugReveal_ : revealProgress;
  const float ease = Smooth(progress);
  // 降下は、遷移が晴れきってから進める(それまでは、落下の最後から落ち着いた姿勢への移りだけ)
  if (ease >= kSettledEase) {
    settledTime_ += deltaTime;
  } else {
    settledTime_ = 0.0f;
  }
  const float descentRatio = debugDescentOverride_ ? debugDescent_ : (descentSeconds_ > 0.0f ? settledTime_ / descentSeconds_ : 1.0f);
  descent_ = Smooth(descentRatio);
  ApplyLook_(ease);
}

void GameOverBackdrop::ApplyLook_(float ease) {
  // 雲: 濃さと光を、落下の最後の状態(ゲームの雲海と同じ)から、落ち着いた状態へ移す
  cloudLayer_ = baseCloud_;
  // 降下が進むほど薄くする(終点で endDensityScale_ 倍)。近い島の色を立たせる
  cloudLayer_.density = Lerp(baseCloud_.density, settledDensity_, ease) * Lerp(1.0f, endDensityScale_, descent_);
  cloudLayer_.sunIntensity = baseCloud_.sunIntensity * Lerp(1.0f, settledSunScale_, ease);
  cloudLayer_.ambientIntensity = baseCloud_.ambientIntensity * Lerp(1.0f, settledAmbientScale_, ease);
  // 復活の飛び立ち: 雲が画面を満たす(濃さと光を、落下の最後 = ゲームの雲海の値へ戻す)
  cloudLayer_.density = Lerp(cloudLayer_.density, baseCloud_.density, cloudFill_);
  cloudLayer_.sunIntensity = Lerp(cloudLayer_.sunIntensity, baseCloud_.sunIntensity, cloudFill_);
  cloudLayer_.ambientIntensity = Lerp(cloudLayer_.ambientIntensity, baseCloud_.ambientIntensity, cloudFill_);

  // カメラ: 雲の層の中(下面から上面までの割合)に置き、層の浅い所から深い所へ沈めながら、真下を見下ろす向きから水平に近い向きへ戻す
  ApplyCamera_(ease, descent_);

  // 膜: 空を塗りつぶす膜は常に同じ。色かぶりは、切り替わった直後は無く、晴れる間に付く(落下の最後と色をそろえるため)
  skyVeil_->SetColor({skyVeilColor_[0], skyVeilColor_[1], skyVeilColor_[2], skyVeilAlpha_});
  // 満たしが進むと、色かぶりが白へ寄り、不透明度も上がる(雲の中が白く飛ぶ)
  const float tintAlpha = settledTintAlpha_ * ease * (1.0f - endTintRelief_ * descent_);
  tint_->SetColor({Lerp(tintColor_[0], 1.0f, cloudFill_), Lerp(tintColor_[1], 1.0f, cloudFill_), Lerp(tintColor_[2], 1.0f, cloudFill_),
                   Lerp(tintAlpha, GameOverCloudDefaults::kFillWhiteAlpha, cloudFill_)});
}

void GameOverBackdrop::ApplyCamera_(float ease, float descent) {
  // 遷移が晴れる間の姿勢: 層の浅い所から深い所へ沈み、真下を見る向きから水平に近い向きへ戻る(方位は +z)
  const float layerRatio = Lerp(startCameraLayerRatio_, cameraLayerRatio_, ease);
  const float eyeY = baseCloud_.layerBottom + (baseCloud_.layerTop - baseCloud_.layerBottom) * layerRatio;
  const float pitch = Lerp(startPitchDeg_, settledPitchDeg_, ease) * kDegToRad;
  Vector3 eye{0.0f, eyeY, 0.0f};
  Vector3 direction{0.0f, std::sin(pitch), std::cos(pitch)};

  if (descent > 0.0f) {
    // 降下の終点: 巣の中心の上、手前(カメラのいる側)へ、高さ / tan(角度) だけ離れた位置。向きは巣の中心
    const Vector3 &nest = islands_.GetNestCenter();
    const float pitchMagnitude = (std::max)(std::fabs(endPitchDeg_), kMinEndPitchMagnitudeDeg) * kDegToRad;
    const float backDistance = endHeightAboveNest_ / std::tan(pitchMagnitude);
    float backX = -nest.x;
    float backZ = -nest.z;
    const float backLength = std::sqrt(backX * backX + backZ * backZ);
    if (backLength > 0.0f) {
      backX /= backLength;
      backZ /= backLength;
    } else {
      backZ = -1.0f; // 巣が原点の真上 / 真下にあるときの、手前の向き(-z)
    }
    const Vector3 endEye{nest.x + backX * backDistance, nest.y + endHeightAboveNest_, nest.z + backZ * backDistance};
    const Vector3 endDirection = Normalize(Vector3{nest.x - endEye.x, nest.y - endEye.y, nest.z - endEye.z});

    // 位置と向きを、降下の進み(始まりと終わりがなめらか)で混ぜる。向きは混ぜたあとに正規化する(方位の差が小さいので十分)
    eye = {Lerp(eye.x, endEye.x, descent), Lerp(eye.y, endEye.y, descent), Lerp(eye.z, endEye.z, descent)};
    direction = Normalize(Vector3{Lerp(direction.x, endDirection.x, descent), Lerp(direction.y, endDirection.y, descent),
                                  Lerp(direction.z, endDirection.z, descent)});
  }

  cameraEye_ = eye;
  cameraTarget_ = {eye.x + direction.x * kLookDistance, eye.y + direction.y * kLookDistance, eye.z + direction.z * kLookDistance};
  // 復活の演出が上書きしているときは、その姿勢を使う(cameraEye_ / cameraTarget_ は、通常の姿勢のまま)
  camera_->SetPerspective(kCameraFovY * fovScale_, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  camera_->SetEye(cameraOverride_ ? overrideEye_ : cameraEye_);
  camera_->SetTarget(cameraOverride_ ? overrideTarget_ : cameraTarget_);
}

void GameOverBackdrop::SetCameraOverride(const Vector3 &eye, const Vector3 &target) {
  cameraOverride_ = true;
  overrideEye_ = eye;
  overrideTarget_ = target;
}

void GameOverBackdrop::Begin() {
  auto *renderer = Renderer::GetInstance();
  renderer->SetCamera(*camera_);
  renderer->BeginRenderScene();
  renderer->DrawSeaSky(seaSky_);
  // 雲の層は、物体を描き終えたあと(EndRenderScene)に描かれる。毎フレーム依頼する。光の向きは空と同じにする
  renderer->AddVolumeCloud(cloudLayer_, seaSky_.SunDirection(), time_);
  // 空を塗りつぶす膜。雲はこのあとで重なるので、この膜は、雲の抜けから見える空だけを雲の中の色にする
  skyVeil_->Draw();
  // 島は、空の膜のあとに描く(膜は深度を持たない全面のスプライトで、先に島を描くと島を塗りつぶす)。
  // 雲の合成が深度を見るので、島は手前の雲の濃さで、もやに溶けて見える
  islands_.Draw(seaSky_.SunDirection());
}

void GameOverBackdrop::ResolveClouds() {
  Renderer::GetInstance()->FlushVolumeClouds();
  // 雲の合成のあと、見出しの前に色かぶりを重ねる(見出しは色かぶりの影響を受けない)
  tint_->Draw();
}

void GameOverBackdrop::End() {
  Renderer::GetInstance()->EndRenderScene(Inverse(camera_->GetProjectionMatrix()));
}

#ifdef USE_IMGUI
void GameOverBackdrop::DrawDebugUi() {
  ImGui::SetNextWindowPos(ImVec2(kDebugWindowX, kDebugWindowY), ImGuiCond_FirstUseEver);
  ImGui::Begin("Game Over Cloud");

  ImGui::SeparatorText("Reveal preview");
  ImGui::Checkbox("Hold reveal (freeze the look)", &debugRevealOverride_);
  ImGui::SliderFloat("Reveal (0 = just after the fall, 1 = settled)", &debugReveal_, 0.0f, 1.0f, "%.2f");

  ImGui::SeparatorText("Descent to the nest (starts when the reveal finishes)");
  ImGui::SliderFloat("Descent seconds", &descentSeconds_, 0.5f, kDescentSliderMax, "%.1f");
  ImGui::SliderFloat("End height above nest", &endHeightAboveNest_, 1.0f, kEndHeightMax, "%.1f");
  ImGui::SliderFloat("End pitch (deg)", &endPitchDeg_, kPitchMinDeg, -1.0f, "%.0f");
  ImGui::Checkbox("Hold descent (see the end pose)", &debugDescentOverride_);
  ImGui::SliderFloat("Descent (0 = start, 1 = end)", &debugDescent_, 0.0f, 1.0f, "%.2f");
  if (ImGui::Button("Replay descent")) {
    settledTime_ = 0.0f;
  }
  ImGui::Text("Time since settled: %.1f s", settledTime_);

  ImGui::SeparatorText("Clarity at the end of the descent (near islands)");
  ImGui::SliderFloat("Tint relief (1 = tint gone at the end)", &endTintRelief_, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Density scale at the end", &endDensityScale_, 0.0f, 1.0f, "%.2f");

  ImGui::SeparatorText("Camera");
  ImGui::SliderFloat("Start layer ratio (0 = bottom, 1 = top)", &startCameraLayerRatio_, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Settled layer ratio", &cameraLayerRatio_, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Start pitch (deg)", &startPitchDeg_, kPitchMinDeg, kPitchMaxDeg, "%.0f");
  ImGui::SliderFloat("Settled pitch (deg)", &settledPitchDeg_, kPitchMinDeg, kPitchMaxDeg, "%.0f");

  ImGui::SeparatorText("Settled cloud (how far you can see)");
  ImGui::SliderFloat("Density", &settledDensity_, 0.0f, kDensityMax, "%.3f");
  ImGui::SliderFloat("Sun scale", &settledSunScale_, 0.0f, kScaleMax, "%.2f");
  ImGui::SliderFloat("Ambient scale", &settledAmbientScale_, 0.0f, kScaleMax, "%.2f");
  ImGui::SliderFloat("Fog density (haze toward the horizon color)", &baseCloud_.fogDensity, 0.0f, 0.02f, "%.4f");

  ImGui::SeparatorText("Light direction");
  ImGui::SliderFloat("Sun azimuth", &seaSky_.sunAzimuthDeg, -180.0f, 180.0f, "%.0f");
  ImGui::SliderFloat("Sun elevation", &seaSky_.sunElevationDeg, -10.0f, 90.0f, "%.0f");

  ImGui::SeparatorText("Color");
  ImGui::ColorEdit3("Sky veil color", skyVeilColor_);
  ImGui::SliderFloat("Sky veil alpha", &skyVeilAlpha_, 0.0f, 1.0f, "%.2f");
  ImGui::ColorEdit3("Tint color", tintColor_);
  ImGui::SliderFloat("Tint alpha (settled)", &settledTintAlpha_, 0.0f, 1.0f, "%.2f");

  // ゲームの雲海と共通の設定(切り替わった直後の状態)。Density / Sun / Ambient は、落下の最後の状態の値で、落ち着いた値は上のスライダー
  if (ImGui::CollapsingHeader("Base cloud (same as the game sea; the state right after the fall)")) {
    VolumeCloudDebugUi::Draw(baseCloud_);
  }

  ImGui::End();
  islands_.DrawDebugUi();
}
#endif
