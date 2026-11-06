#include "RebirthButterfly.h"

#include "AbsoluteEngine/resources/AssetManager.h"
#include "AnimationManager.h"
#include "GameAssetPreloader.h"
#include "Logger.h"
#include "Method.h"
#include "ModelResource.h"
#include "PlayerDeathDirector.h"
#include "Renderer.h"
#include "TextureResource.h"
#include "actor/Player/PlayerModelDefs.h"
#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 翼幅(ゲームシーンの自機と同じ。GameScene の kPlayerWingspan)
constexpr float kWingspan = 3.7f;
// 幅が小さすぎるモデルで、拡大が無限大にならないための下限
constexpr float kMinFitWidth = 1.0e-4f;
// 復活のアニメ(現れ、溜め、バースト、落ち着き。1 回再生。タイムラインが、現在の時刻を毎フレーム指定する)の名前と、
// まだ無いときの代役(翅が広がるアニメ)の名前
constexpr const char *kReviveAnimation = "revive";
constexpr const char *kStandInAnimation = "emerge";
// 胴体のボーン名(頭、腹、翅の根元の親。胸のあたりに原点がある)
constexpr const char *kBodyBone = "body";
// 羽ばたきへつなぐときのクロスフェード(秒)。落ち着きの最後の姿勢から、羽ばたきの先頭の姿勢へ
constexpr float kFlapBlendSeconds = 0.15f;
// 蝶の翅は薄く、両面が見える。光の当たらない側が真っ黒にならないよう、環境光つきの薄い面の照明にする
constexpr int32_t kThinSurface = ModelLightingMode::kThinSurface;
} // namespace

bool RebirthButterfly::Initialize() {
  auto *assets = AbsoluteEngine::AssetManager::GetInstance();
  auto fail = [this](const char *reason) {
    loadFailure_ = reason;
    Logger::GetInstance().Log("[RebirthButterfly] initialize failed: %s", reason);
    return false;
  };
  auto resource = assets->Load<ModelResource>(GameAssetPaths::kPlayerModel);
  if (!resource) return fail("model resource not loaded");

  ModelInstance::CreateInfo info{};
  info.resource = resource;
  info.lightingMode = kThinSurface;
  if (!model_.Initialize(info)) return fail("ModelInstance::Initialize failed");

  // 翼幅 kWingspan に合わせる拡大と、中心(ModelComponent::FitToWingspan と同じ求め方)
  const std::vector<Vector3> &positions = resource->GetVertexPositions();
  if (!positions.empty()) {
    Vector3 lo = positions[0];
    Vector3 hi = positions[0];
    for (const Vector3 &p : positions) {
      lo = {(std::min)(lo.x, p.x), (std::min)(lo.y, p.y), (std::min)(lo.z, p.z)};
      hi = {(std::max)(hi.x, p.x), (std::max)(hi.y, p.y), (std::max)(hi.z, p.z)};
    }
    fitCenter_ = {(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
    const float width = hi.x - lo.x;
    fitScale_ = width > kMinFitWidth ? kWingspan / width : 1.0f;
  }

  // ディゾルブのマスク(蝶専用。翅の外側ほど先に消える = 逆再生では、胴から先に現れ、翅の外側が最後)
  if (auto mask = assets->Load<TextureResource>(GameAssetPaths::kPlayerDissolveMask)) {
    model_.SetDissolveMaskTexture(mask);
  }

  // アニメ: "dir/file.gltf" を、ディレクトリとファイル名に分けて読む(ModelComponent::PlayAnimation と同じ)
  const std::string path = GameAssetPaths::kPlayerModel;
  const size_t slash = path.find_last_of('/');
  const std::string directory = slash == std::string::npos ? std::string{} : path.substr(0, slash);
  const std::string filename = slash == std::string::npos ? path : path.substr(slash + 1);
  const auto animations = AnimationManager::GetInstance()->LoadAnimationSet(directory, filename);
  if (!animations) return fail("animation set not loaded");
  revive_ = animations->Find(kReviveAnimation);
  usingStandIn_ = revive_ == nullptr;
  if (usingStandIn_) revive_ = animations->Find(kStandInAnimation); // revive が届くまでの代役
  flap_ = animations->Find(PlayerAnimation::kFlap);
  if (!revive_ || !flap_) return fail("animation revive (or emerge) / Flap not found");

  // 最初は、アニメの 0 秒のポーズで止める。以後、時刻はタイムラインが指定する
  model_.PlayAnimation(revive_, false);
  model_.UpdateAnimation(0.0f);
  loaded_ = true;
  loadFailure_ = "none";
  return true;
}

void RebirthButterfly::Update(float deltaTime, const RebirthState &state) {
  visible_ = loaded_ && state.butterflyVisible;
  lastPosition_ = state.butterflyPosition;
  lastScale_ = state.butterflyScale;
  lastThreshold_ = state.dissolveThreshold;
  lastDissolve_ = state.dissolveEnabled;
  lastFlapping_ = state.flapping;
  lastReviveSeconds_ = state.reviveSeconds;
  if (!visible_) return;

  // アニメ: 現れから落ち着きまでは、タイムラインが指定した時刻のポーズにする(時間を足して進めるのではなく、時刻を直接置く。
  // ヒットストップや段階の長さの変更に関係なく、段階とアニメの区間がずれないため)。飛び立つ段階で、羽ばたきへ
  if (state.flapping) {
    if (!flapStarted_) {
      model_.PlayAnimation(flap_, true, kFlapBlendSeconds);
      flapStarted_ = true;
    }
    model_.UpdateAnimation(deltaTime);
  } else if (state.reviveActive && state.reviveLengthSeconds > 0.0f) {
    // タイムラインの時刻(0〜想定の長さ)を、読み込んだアニメの長さへ直す(代役のアニメは長さが違う)
    const float scaleToAnimation = revive_->duration / state.reviveLengthSeconds;
    model_.SetAnimationTime(state.reviveSeconds * scaleToAnimation);
  }

  // 位置・向き・大きさ(ModelComponent::ComputeWorldMatrix_ と同じ順: 中心を原点へ -> 翼幅の拡大 -> 拡大・回転・位置)
  const Matrix4x4 fit = Multiply(MakeTranslateMatrix({-fitCenter_.x, -fitCenter_.y, -fitCenter_.z}),
                                 MakeScaleMatrix({fitScale_, fitScale_, fitScale_}));
  const Vector3 scale{state.butterflyScale, state.butterflyScale, state.butterflyScale};
  world_ = Multiply(fit, MakeAffineMatrix(scale, state.butterflyRotate, state.butterflyPosition));
  model_.SetWorld(world_);

  // 翅の閃光: 明るさに倍率を一瞬だけ足し、玉虫色(青緑寄り)へ寄せる
  const float flash = state.flashAmount;
  model_.SetColor({brightness_ * (1.0f + flash * state.flashColor.x), brightness_ * (1.0f + flash * state.flashColor.y),
                   brightness_ * (1.0f + flash * state.flashColor.z), 1.0f});

  // ディゾルブの逆再生。縁の色と熾火は、自機の崩壊と同じ
  const Vector3 edgeColor{PlayerDeathDefaults::kDissolveEdgeColorR, PlayerDeathDefaults::kDissolveEdgeColorG,
                          PlayerDeathDefaults::kDissolveEdgeColorB};
  model_.SetDissolveParam(state.dissolveEnabled, state.dissolveThreshold, state.dissolveEdgeRange, edgeColor, {1.0f, 1.0f, 1.0f},
                          PlayerDeathDefaults::kDissolveEmberAmount);
}

bool RebirthButterfly::GetBodyPosition(Vector3 &out) const {
  if (!visible_) return false;
  // ボーンの位置は、アニメで動く(畳んだ姿勢では沈み、翅を広げると上がる)ので、毎フレーム、今のポーズから求める。
  // 相対位置(蝶の位置 + 縮尺 x 固定のずれ)では、この動きに付いていけない
  Matrix4x4 boneModel;
  if (!model_.TryGetBoneModelMatrix(kBodyBone, boneModel)) return false;
  const Matrix4x4 boneWorld = Multiply(boneModel, world_);
  out = {boneWorld.m[3][0], boneWorld.m[3][1], boneWorld.m[3][2]};
  return true;
}

void RebirthButterfly::Draw() {
  if (!visible_) return;
  // ModelInstance::Draw は、スキニング(頂点を骨で変形する compute)を回してから描く。Renderer::DrawModel だけを呼ぶと、
  // 変形後の頂点バッファが空のままで、何も映らない(ゲームの自機も ModelComponent 経由で Draw を呼ぶ)
  model_.Draw();
  ++drawCount_;
}

#ifdef USE_IMGUI
void RebirthButterfly::DrawDebugUi() {
  ImGui::SeparatorText("Butterfly (debug)");
  ImGui::Text("Loaded: %s (%s)", loaded_ ? "yes" : "NO", loadFailure_);
  ImGui::Text("Visible: %s   Draw calls: %d", visible_ ? "yes" : "no", drawCount_);
  ImGui::Text("Position: %.1f, %.1f, %.1f   Scale: %.2f", lastPosition_.x, lastPosition_.y, lastPosition_.z, lastScale_);
  ImGui::Text("Dissolve: %s  threshold %.2f", lastDissolve_ ? "on" : "off", lastThreshold_);
  ImGui::SliderFloat("Brightness", &brightness_, 0.5f, 4.0f, "%.2f");
  ImGui::Text("Animation: %s   revive time %.2f s", lastFlapping_ ? "Flap" : (usingStandIn_ ? "emerge (stand-in for revive)" : "revive"),
              lastReviveSeconds_);
}
#endif
