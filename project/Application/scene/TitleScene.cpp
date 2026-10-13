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
#include "ParticleManager.h"
#include "ModelResource.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
// 本編の描画先と平面反射の描画先の大きさ(同じにしないと、反射が画面座標とずれる)
constexpr uint32_t kSceneWidth = 1280;
constexpr uint32_t kSceneHeight = 720;
// 水面の高さ(ワールド y)。Water.hlsli の kWaterHeight と同じ値にすること
constexpr float kWaterSurfaceHeight = 0.0f;

constexpr double kDeltaTime = 1.0 / 60.0;                 // time_ 用(double)
constexpr float kDeltaTimeF = static_cast<float>(kDeltaTime); // float のメンバや関数へ渡す用

// カメラの投影(位置・向き・画角は TitleDirector が決める)
constexpr float kCameraNear = 0.1f;
constexpr float kCameraFar = 1000.0f;

constexpr float kLogoFadeInSec = 2.0f;

// スタート演出(加速 → 白フラッシュ → ゲームへ)
// 【仮設定】展示でビルドを見せるため、true の間は演出のあとゲームへ行かずタイトルへ戻る(ゲームから戻る手段が
// 今は「死ぬこと」しかないため)。ゲームへ遷移させたいときは false にする
constexpr bool kReturnToTitleAfterLaunch = true;
constexpr float kReturnFadeSec = 0.6f; // タイトルへ戻ったあと、白から元の画面へ戻る時間
constexpr float kLaunchDurationSec = 2.4f;
constexpr float kFlashStartRatio = 0.65f; // 演出のこの割合から白くなり始め、終わりで真っ白

// PRESS START の明滅(ゆっくり。完全には消えない)
constexpr float kTwoPi = 6.2831853f;
constexpr float kStartBlinkPeriodSec = 3.0f;
constexpr float kStartBlinkMinAlpha = 0.25f;

// 太陽の方向(太陽へ向かう向き)。空の描画で使う
const Vector3 kSunDirection{0.30f, 0.22f, 1.0f};

// ライティングモード(0 = ライトの影響を受けず、テクスチャの色をそのまま出す)。
// タイトルにはライトオブジェクトが無く、ライトを足すと白いテクスチャが飛んだり、影側が真っ黒になったりするため、
// 自機もロゴ類と同じく 0 にする
constexpr int32_t kUnlitMode = 0;

constexpr const char* kPlayerPath = "resources/app/player/dragon/dragon.obj";
constexpr const char* kLogoPath = "resources/app/title/logo.obj";
constexpr const char* kStartPath = "resources/app/title/start.obj";
constexpr const char* kFlashPath = "resources/app/title/flash.obj";
constexpr const char* kStreakPath = "resources/app/title/streak.obj";

// 翼のプルーム(パーティクル)
constexpr const char* kContrailGroup = "contrail";
constexpr const char* kContrailTexture = "resources/app/title/puff.png";
constexpr uint32_t kContrailMaxParticles = 4096;
// 発生点: 自機モデルの右翼の先端 3 か所(左翼は中心を挟んで反転)。位置はモデルの頂点から自動で求める(FitPlayerModel)。
// 下は先端ごとの粒の大きさ・濃さの倍率(一番外側の先端が最も強い)
constexpr float kContrailTipWeights[kContrailTipCount] = {1.0f, 0.6f, 0.5f};

// 自機の大きさ: 翼幅(モデルの x 方向の幅)がこの値になるよう拡大する(ワールドの単位)。モデルを差し替えても大きさが揃う
constexpr float kPlayerWingspan = 3.7f;
// 翼の頂点とみなす範囲: 中心から x 方向の最大幅のこの割合より外側
// (0.6 だと内側の後ろ脚のフィンの先まで入ってしまうので、翼の外側 20% に絞る)
constexpr float kWingRegionRatio = 0.8f;

struct PlayerModelFit {
  Vector3 center{0.0f, 0.0f, 0.0f};
  float scale = 1.0f;
  std::array<Vector3, kContrailTipCount> rightWingTips{};
};

float DistanceBetween(const Vector3 &a, const Vector3 &b) {
  return Length({a.x - b.x, a.y - b.y, a.z - b.z});
}

// モデルの頂点から、大きさ(翼幅を kPlayerWingspan に)、中心(バウンディングボックスの中心)、右翼の先端を求める。
// 先端は、翼の領域(中心から x 方向に外側 20%)の頂点から、「一番外側 → それから一番遠い点 → 両方から遠い点」の順に選ぶ
PlayerModelFit FitPlayerModel(const std::vector<Vector3> &positions) {
  PlayerModelFit fit;
  if (positions.empty()) return fit;

  Vector3 lo = positions[0], hi = positions[0];
  for (const Vector3 &p : positions) {
    lo = {(std::min)(lo.x, p.x), (std::min)(lo.y, p.y), (std::min)(lo.z, p.z)};
    hi = {(std::max)(hi.x, p.x), (std::max)(hi.y, p.y), (std::max)(hi.z, p.z)};
  }
  fit.center = {(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
  const float wingspan = hi.x - lo.x;
  fit.scale = wingspan > 1e-4f ? kPlayerWingspan / wingspan : 1.0f;

  const float wingStartX = fit.center.x + (hi.x - fit.center.x) * kWingRegionRatio;
  std::vector<Vector3> wing;
  for (const Vector3 &p : positions) {
    if (p.x >= wingStartX) wing.push_back(p);
  }
  if (wing.empty()) return fit;

  // 1 本目: 一番外側(x 最大)
  fit.rightWingTips[0] = *std::max_element(wing.begin(), wing.end(), [](const Vector3 &a, const Vector3 &b) { return a.x < b.x; });
  // 2 本目以降: すでに選んだ先端のどれからも遠い点
  for (int k = 1; k < kContrailTipCount; ++k) {
    float bestDistance = -1.0f;
    for (const Vector3 &p : wing) {
      float nearest = 1e30f;
      for (int j = 0; j < k; ++j) {
        nearest = (std::min)(nearest, DistanceBetween(p, fit.rightWingTips[j]));
      }
      if (nearest > bestDistance) {
        bestDistance = nearest;
        fit.rightWingTips[k] = p;
      }
    }
  }
  return fit;
}
constexpr float kContrailSpacing = 0.25f;      // 粒を置く間隔(ワールドの距離)
constexpr int kContrailMaxPerFramePerPoint = 4; // 加速中でも粒が増えすぎないための上限
constexpr float kContrailLifetimeSec = 1.6f;
constexpr float kContrailStartScale = 0.10f;
constexpr float kContrailEndScale = 0.65f;
constexpr float kContrailAlpha = 0.55f;
constexpr float kContrailSideDrift = 0.20f;    // 外側へゆっくり広がる速さ
constexpr float kContrailRise = 0.06f;
constexpr float kContrailJitter = 0.05f;       // 粒の位置・速度のばらつき

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

void TitleScene::RestartTitle() {
  // 画面が真っ白な瞬間に、演出の状態を最初へ戻す(自機の位置・水の波・プルームも最初から)
  startRequested_ = false;
  launchTime_ = 0.0f;
  time_ = 0.0;
  lastWorldOffsetZ_ = 0.0f;
  contrailHasPrev_ = false;
  ParticleManager::GetInstance()->ClearParticleGroup(kContrailGroup);
  returnFadeTime_ = 0.0f; // ここから白がフェードアウトする
}

void TitleScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  startRequested_ = false;
  leaving_ = false;
  launchTime_ = 0.0f;
  flashAlpha_ = 0.0f;
  returnFadeTime_ = kReturnFadeSec; // 起動直後は白からのフェードはしない
  time_ = 0.0;
  lastWorldOffsetZ_ = 0.0f;
  Renderer::GetInstance()->InitializePostProcess(kSceneWidth, kSceneHeight);
  Renderer::GetInstance()->InitializePlanarReflection(kSceneWidth, kSceneHeight);

  auto camera = std::make_shared<GameCamera>();
  camera->Initialize();
  const TitleState initial = director_.Evaluate(0.0f);
  camera->SetPerspective(initial.fovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  camera->SetEye(initial.cameraEye);
  camera->SetTarget(initial.cameraTarget);
  SetMainCamera(camera);

  InitModel(playerModel_, kPlayerPath, kUnlitMode);
  // 自機モデルの大きさ・中心・翼の先端を、頂点から求める(モデルを差し替えても合う)
  if (auto *playerResource = playerModel_.GetResource()) {
    const PlayerModelFit fit = FitPlayerModel(playerResource->GetVertexPositions());
    director_.SetPlayerModelFit(fit.center, fit.scale);
    modelCenter_ = fit.center;
    wingTips_ = fit.rightWingTips;
  }
  InitModel(logoModel_, kLogoPath, kUnlitMode);
  InitModel(startModel_, kStartPath, kUnlitMode);
  InitModel(flashModel_, kFlashPath, kUnlitMode);
  for (auto &streak : streakModels_) {
    InitModel(streak, kStreakPath, kUnlitMode);
  }

  // 翼のプルーム。同名のグループが残っていれば、ParticleManager がリセットして再利用する
  ParticleManager::GetInstance()->CreateParticleGroup(kContrailGroup, kContrailTexture, kContrailMaxParticles);
  contrailHasPrev_ = false;
}

void TitleScene::Finalize() {
  // プルームの粒がゲームシーンへ持ち越されないようにする
  ParticleManager::GetInstance()->ClearParticleGroup(kContrailGroup);
}

void TitleScene::EmitContrails(const Matrix4x4 &playerWorld) {
  auto *particles = ParticleManager::GetInstance();
  // 粒ごとの小さなばらつきを作る(疑似乱数。-1〜1)
  auto random = [this]() {
    contrailSeed_ = contrailSeed_ * 1664525u + 1013904223u;
    return static_cast<float>(contrailSeed_ >> 8) * (2.0f / 16777216.0f) - 1.0f;
  };

  int index = 0;
  for (const float side : {1.0f, -1.0f}) {
    for (int k = 0; k < kContrailTipCount; ++k) {
      // 右翼の先端を、モデルの中心を挟んで x 方向に反転すると左翼の先端
      const Vector3 &right = wingTips_[k];
      const Vector3 local{side > 0.0f ? right.x : 2.0f * modelCenter_.x - right.x, right.y, right.z};
      const Vector3 tip = TransformPoint(local, playerWorld);
      const Vector3 prev = contrailHasPrev_ ? contrailPrevTip_[index] : tip;

      // 前フレームの先端位置から今の位置まで、一定間隔で粒を置く(速く動いても軌跡が切れない)
      const float distance = Length({tip.x - prev.x, tip.y - prev.y, tip.z - prev.z});
      const int count = (std::min)((std::max)(static_cast<int>(std::ceil(distance / kContrailSpacing)), 1), kContrailMaxPerFramePerPoint);
      for (int k = 1; k <= count; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(count);
        const Vector3 position{prev.x + (tip.x - prev.x) * t + random() * kContrailJitter,
                               prev.y + (tip.y - prev.y) * t + random() * kContrailJitter,
                               prev.z + (tip.z - prev.z) * t + random() * kContrailJitter};
        const Vector3 velocity{side * kContrailSideDrift + random() * kContrailJitter, kContrailRise + random() * kContrailJitter, random() * kContrailJitter};
        const float w = kContrailTipWeights[k];
        const Vector3 startScale{kContrailStartScale * w, kContrailStartScale * w, kContrailStartScale * w};
        const Vector3 endScale{kContrailEndScale * w, kContrailEndScale * w, kContrailEndScale * w};
        particles->Emit(kContrailGroup, position, velocity, startScale, Vector3{0.0f, 0.0f, 0.0f}, kContrailLifetimeSec,
                        Vector4{1.0f, 1.0f, 1.0f, kContrailAlpha * w}, &endScale);
      }
      contrailPrevTip_[index] = tip;
      ++index;
    }
  }
  contrailHasPrev_ = true;
}

void TitleScene::Update() {
  time_ += kDeltaTime;

  if (startRequested_) {
    launchTime_ += kDeltaTimeF;
  }
  // 演出が終わった(画面が真っ白になった)フレームで、タイトルを最初へ戻す(仮設定)か、一度だけゲームへ遷移する
  if (startRequested_ && launchTime_ >= kLaunchDurationSec) {
    if (kReturnToTitleAfterLaunch) {
      RestartTitle();
    } else if (!leaving_) {
      leaving_ = true;
      RequestSceneChange(SceneId::Game);
    }
  }

  const float launchU = startRequested_ ? (std::min)(launchTime_ / kLaunchDurationSec, 1.0f) : -1.0f;
  flashAlpha_ = startRequested_ ? CalcFlashAlpha(launchU) : 0.0f;
  // タイトルへ戻った直後は、白から元の画面へなめらかに戻す
  if (!startRequested_ && returnFadeTime_ < kReturnFadeSec) {
    returnFadeTime_ += kDeltaTimeF;
    const float t = (std::min)(returnFadeTime_ / kReturnFadeSec, 1.0f);
    flashAlpha_ = 1.0f - t * t * (3.0f - 2.0f * t);
  }

  const TitleState state = director_.Evaluate(time_, launchU);

  // 描画用のワールド座標が巻き戻されたら(自機が一定距離進むたび)、残っているプルームの粒と、前フレームの先端位置も
  // 同じだけずらして合わせる(そうしないと、巻き戻しの瞬間に軌跡が飛ぶ)
  if (state.worldOffset.z != lastWorldOffsetZ_) {
    const float shiftZ = lastWorldOffsetZ_ - state.worldOffset.z;
    ParticleManager::GetInstance()->TranslateParticleGroup(kContrailGroup, Vector3{0.0f, 0.0f, shiftZ});
    for (Vector3 &tip : contrailPrevTip_) {
      tip.z += shiftZ;
    }
    lastWorldOffsetZ_ = state.worldOffset.z;
  }
  worldOffset_ = state.worldOffset;
  worldWrapLength_ = state.worldWrapLength;
  GetMainCamera()->SetPerspective(state.fovY, Renderer::GetInstance()->GetAspectRatio(), kCameraNear, kCameraFar);
  GetMainCamera()->SetEye(state.cameraEye);
  GetMainCamera()->SetTarget(state.cameraTarget);

  playerModel_.SetWorld(state.playerWorld);

  logoModel_.SetWorld(state.logoWorld);
  logoModel_.SetColor({1.0f, 1.0f, 1.0f, CalcLogoAlpha(static_cast<float>(time_))});

  startModel_.SetWorld(state.startWorld);
  startModel_.SetColor({1.0f, 1.0f, 1.0f, CalcBlinkAlpha(static_cast<float>(std::fmod(time_, static_cast<double>(kStartBlinkPeriodSec))))});

  flashModel_.SetWorld(state.flashWorld);
  flashModel_.SetColor({1.0f, 1.0f, 1.0f, flashAlpha_});

  for (int i = 0; i < kTitleStreakCount; ++i) {
    streakModels_[i].SetWorld(state.streakWorld[i]);
    streakModels_[i].SetColor({1.0f, 1.0f, 1.0f, state.streakAlpha[i]});
  }

  EmitContrails(state.playerWorld);

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

void TitleScene::Draw() {
  auto* renderer = Renderer::GetInstance();
  renderer->SetCamera(*GetMainCamera());

  // 水面に映る物体を、本編の描画より先に反射用の描画先へ描く(自機だけ。ロゴ類・プルーム・気流は映さない)
  renderer->BeginReflectionPass(kWaterSurfaceHeight);
  renderer->DrawModel(&playerModel_);
  renderer->EndReflectionPass();

  renderer->BeginRenderScene();

  // 空は最奥に描くので順番は自由だが、不透明物の前に描いて読みやすくする
  // 時間は巻き戻さずそのまま渡す(巻き戻すと、その瞬間に波と雲がワープする。float の精度は 10 時間でも位相誤差 0.01rad 未満)
  const float shaderTime = static_cast<float>(time_);
  renderer->DrawSky(Normalize(kSunDirection), shaderTime, worldOffset_);
  renderer->DrawWater(Normalize(kSunDirection), shaderTime, worldOffset_, worldWrapLength_);

  renderer->DrawModel(&playerModel_);

  // 翼のプルーム: 粒の行列は今フレームのカメラで作りたいので、SetCamera の後にここで更新する。
  // 深度テストあり(書き込みなし)で、自機の後ろに回った粒は自機に隠れる
  auto *particles = ParticleManager::GetInstance();
  particles->Update(kDeltaTimeF);
  particles->Draw(BlendMode::Alpha, true);

  // 気流の筋(深度テストあり。自機より後ろの筋は自機に隠れる)
  for (auto &streak : streakModels_) {
    renderer->DrawEffectModel(&streak);
  }

  // 半透明(depthWrite なし)のロゴ類は不透明物の後に描く。カメラのすぐ前に置いてあるので自機より常に手前
  renderer->DrawEffectModel(&logoModel_);
  renderer->DrawEffectModel(&startModel_);
  // 白フラッシュは一番最後(一番手前)に描く
  if (flashAlpha_ > 0.0f) {
    renderer->DrawEffectModel(&flashModel_);
  }

  renderer->EndRenderScene(Inverse(GetMainCamera()->GetProjectionMatrix()));
}
