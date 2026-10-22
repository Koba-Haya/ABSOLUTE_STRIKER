#define NOMINMAX
#include "StageNameClouds.h"

#include "AbsoluteEngine/base/AssetPath.h"
#include "ModelResource.h"
#include "ModelUtils.h"
#include "ParticleManager.h"
#include "Renderer.h"
#include "TextureResource.h"
#include "externals/DirectXTex/DirectXTex.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>

namespace {
// ライティングモード 0: ライトの影響を受けず、テクスチャの色をそのまま出す
constexpr int32_t kUnlitMode = 0;
// パフの数の上限(多すぎて、頂点バッファが大きくなりすぎるのを防ぐ)
constexpr size_t kMaxPuffs = 2500;
// 文字の形を拾うとき、この値以上のアルファを、文字の一部とみなす(0 〜 255)
constexpr uint8_t kMaskThreshold = 128;
// パフの画像は、中心が濃く、外へ向かってなだらかに薄くなる。描画は、アルファが 0.5 より大きい所だけを描くので、
// 見える円の半径は、四角形の半径の半分になる。見える半径から四角形の半径を求める倍率
constexpr float kQuadScale = 2.0f;
// 本体の円の、影に対する大きさの割合(影を、本体の縁からはみ出させて、立体感を出す)
constexpr float kBodyScale = 0.92f;
// 影を、パフの半径の何割、右下へずらすか(画面の右と下へ)。
// 大きくずらして、はっきりした影にすると、文字の輪郭が、雲だけで読める(輪郭線を重ねなくてよい)
constexpr float kShadowShiftRight = 0.45f;
constexpr float kShadowShiftDown = 0.8f;
// 霧の 2 個目以降の粒(細かくちぎれた雲)の、速さと大きさの、1 個目に対する倍率
constexpr float kFragmentSpeedRatio = 1.4f;
constexpr float kFragmentScaleRatio = 0.6f;
// 影を奥へ、光を手前へ、ずらす距離(ユニット。カメラの前方向)。同じ深さで重なって、ちらつかないようにする
constexpr float kLayerDepthGap = 0.04f;
// 光の円の、パフの半径に対する割合の上限(光の強さが最大のとき)
constexpr float kGlowScale = 0.95f;
// 影・本体・光の色
constexpr Vector4 kShadowColor{0.50f, 0.59f, 0.78f, 1.0f};
constexpr Vector4 kBodyColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr Vector4 kGlowColor{1.0f, 0.93f, 0.70f, 0.75f};
// 文字が組み上がったあとの、パフの揺れ(ユニットと、角速度ラジアン / 秒)
constexpr float kWobbleAmplitude = 0.02f;
constexpr float kWobbleSpeed = 2.0f;
// ease-out-back の係数(少し行き過ぎてから、戻る。パフが育つときの弾み)
constexpr float kBackOvershoot = 1.70158f;
// 揺れの位相・出現のずれの乱数の種
constexpr uint32_t kRandomSeed = 12345u;
constexpr float kTwoPi = 6.2831853f;

float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float EaseOut(float t) {
  const float c = Clamp01(t);
  return 1.0f - (1.0f - c) * (1.0f - c) * (1.0f - c);
}
float EaseOutBack(float t) {
  const float c = Clamp01(t);
  const float c3 = kBackOvershoot + 1.0f;
  const float u = c - 1.0f;
  return 1.0f + c3 * u * u * u + kBackOvershoot * u * u;
}

// ステージ名の画像から、アルファ(0 〜 255)だけを読み取る。
// 戻り値: 読み取れたら true。alpha は、上の行から順に、横に並べた width x height 個
bool LoadAlphaMask(const char *assetPath, std::vector<uint8_t> &alpha, size_t &width, size_t &height) {
  const std::filesystem::path resolved = AbsoluteEngine::AssetPath::Resolve(assetPath);
  DirectX::ScratchImage image;
  if (FAILED(DirectX::LoadFromWICFile(resolved.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image))) {
    return false;
  }

  const DirectX::Image *source = image.GetImage(0, 0, 0);
  if (source == nullptr) {
    return false;
  }

  // アルファが 4 バイト目にある 8 ビットの形式でなければ、変換する
  DirectX::ScratchImage converted;
  const bool isEightBitRgba = source->format == DXGI_FORMAT_R8G8B8A8_UNORM || source->format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                              source->format == DXGI_FORMAT_B8G8R8A8_UNORM || source->format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  if (!isEightBitRgba) {
    if (FAILED(DirectX::Convert(*source, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT,
                                converted))) {
      return false;
    }
    source = converted.GetImage(0, 0, 0);
    if (source == nullptr) {
      return false;
    }
  }

  width = source->width;
  height = source->height;
  alpha.resize(width * height);
  for (size_t y = 0; y < height; ++y) {
    const uint8_t *row = source->pixels + y * source->rowPitch;
    for (size_t x = 0; x < width; ++x) {
      alpha[y * width + x] = row[x * 4 + 3];
    }
  }
  return true;
}
} // namespace

// 1 回の描画(影・本体・光のどれか)分の、頂点バッファと、描画の設定
struct StageNameClouds::Layer {
  std::vector<VertexData> vertices; // パフ 1 つにつき 4 頂点。位置は毎フレーム書き換える
  std::shared_ptr<ModelResource> resource;
  ModelInstance instance;
};

StageNameClouds::StageNameClouds() = default;
StageNameClouds::~StageNameClouds() = default;

bool StageNameClouds::BuildLayer_(Layer &layer, const std::shared_ptr<TextureResource> &puffTexture) {
  const size_t count = puffs_.size();

  // パフごとの四角形の、画像の座標(左上 = (0, 0))と、インデックス。位置は、描くたびに書き換える
  const Vector2 uvs[4] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
  MeshData mesh;
  mesh.vertices.reserve(count * 4);
  mesh.indices.reserve(count * 6);
  for (size_t i = 0; i < count; ++i) {
    for (const Vector2 &uv : uvs) {
      VertexData vertex;
      vertex.position = {0.0f, 0.0f, 0.0f, 1.0f};
      vertex.texcoord = uv;
      vertex.normal = {0.0f, 0.0f, -1.0f};
      mesh.vertices.push_back(vertex);
    }
    const uint32_t base = static_cast<uint32_t>(i * 4);
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }
  layer.vertices = mesh.vertices;

  auto modelData = std::make_shared<ModelData>();
  modelData->meshes.push_back(std::move(mesh));

  layer.resource = std::make_shared<ModelResource>();
  ModelResource::CreateInfo createInfo{};
  createInfo.dx = Renderer::GetInstance()->GetDX();
  createInfo.modelData = modelData;
  createInfo.texture = puffTexture;
  if (!layer.resource->Initialize(createInfo)) {
    return false;
  }

  ModelInstance::CreateInfo instanceInfo{};
  instanceInfo.resource = layer.resource;
  instanceInfo.lightingMode = kUnlitMode;
  return layer.instance.Initialize(instanceInfo);
}

bool StageNameClouds::Build(const char *maskImagePath, const std::shared_ptr<TextureResource> &puffTexture, float halfWidth,
                            float halfHeight, const Vector2 &impact, const StageCloudSettings &settings, const Vector2 *sweepRangeX) {
  built_ = false;
  broken_ = false;
  breakElapsed_ = 0.0f;
  reveal_ = 0.0f;
  settings_ = settings;
  impact_ = impact;
  puffs_.clear();
  if (!puffTexture || settings.puffSpacing <= 0.0f) {
    return false;
  }

  std::vector<uint8_t> alpha;
  size_t maskWidth = 0;
  size_t maskHeight = 0;
  if (!LoadAlphaMask(maskImagePath, alpha, maskWidth, maskHeight) || maskWidth == 0 || maskHeight == 0) {
    return false;
  }

  // 板の面を、間隔 puffSpacing の格子で区切り、文字の部分(画像の不透明な所)の格子点へ、パフを置く。
  // 格子の規則的な並びが見えないよう、位置を少しずらす
  std::mt19937 random(kRandomSeed);
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const float spacing = settings.puffSpacing;

  // 板の座標 (x, y) が、画像の文字の部分(不透明な所)に入るか。板の座標から画像の座標へ(左端 = 0、上端 = 0。板の y は上が正なので反転する)
  const auto isInk = [&](float x, float y) {
    const float u = (x + halfWidth) / (2.0f * halfWidth);
    const float v = 1.0f - (y + halfHeight) / (2.0f * halfHeight);
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
      return false;
    }
    const size_t px = std::min(static_cast<size_t>(u * static_cast<float>(maskWidth - 1)), maskWidth - 1);
    const size_t py = std::min(static_cast<size_t>(v * static_cast<float>(maskHeight - 1)), maskHeight - 1);
    return alpha[py * maskWidth + px] >= kMaskThreshold;
  };

  float minX = halfWidth;
  float maxX = -halfWidth;
  for (float y = -halfHeight + spacing * 0.5f; y < halfHeight && puffs_.size() < kMaxPuffs; y += spacing) {
    for (float x = -halfWidth + spacing * 0.5f; x < halfWidth && puffs_.size() < kMaxPuffs; x += spacing) {
      const float jx = x + (unit(random) - 0.5f) * spacing * 0.5f;
      const float jy = y + (unit(random) - 0.5f) * spacing * 0.5f;

      // 雲にする範囲(画像の上端を 0、下端を 1 として、maskTopV 〜 maskBottomV)の外は置かない
      const float v = 1.0f - (jy + halfHeight) / (2.0f * halfHeight);
      if (v < settings.maskTopV || v > settings.maskBottomV || !isInk(jx, jy)) {
        continue;
      }

      // パフの半径を決め、中心から半径 x edgeInset だけ離れた上下左右の 4 点も、文字の中に入るときだけ置く
      // (パフが文字の縁からはみ出して、文字が太り、「A」や「G」の穴がつぶれるのを防ぐ)
      const float radius = settings.minPuffRadius + (settings.maxPuffRadius - settings.minPuffRadius) * unit(random);
      const float inset = radius * settings.edgeInset;
      if (!isInk(jx + inset, jy) || !isInk(jx - inset, jy) || !isInk(jx, jy + inset) || !isInk(jx, jy - inset)) {
        continue;
      }

      Puff puff;
      puff.target = {jx, jy};
      puff.radius = radius;
      puff.phase = unit(random) * kTwoPi;
      // 出現のとき、板の面に沿ってずれた向きと、奥へずれた向き(奥 = 板の前方向 +z)
      const float angle = unit(random) * kTwoPi;
      const float distance = settings.startOffset * unit(random);
      puff.startOffset = {std::cos(angle) * distance, std::sin(angle) * distance, settings.startDepth * (0.5f + 0.5f * unit(random))};
      puffs_.push_back(puff);
      minX = std::min(minX, jx);
      maxX = std::max(maxX, jx);
    }
  }
  if (puffs_.empty()) {
    return false;
  }

  // 光の筋が通る順(筋が通る範囲の左端を 0、右端を 1)。範囲を渡されたら、それを使う(別の文字と、筋の位置をそろえる)
  textRangeX_ = {minX, maxX};
  const float sweepMin = sweepRangeX != nullptr ? sweepRangeX->x : minX;
  const float sweepMax = sweepRangeX != nullptr ? sweepRangeX->y : maxX;
  const float span = std::max(sweepMax - sweepMin, 1e-4f);
  for (Puff &puff : puffs_) {
    puff.xNorm = (puff.target.x - sweepMin) / span;
  }

  shadow_ = std::make_unique<Layer>();
  body_ = std::make_unique<Layer>();
  glow_ = std::make_unique<Layer>();
  if (!BuildLayer_(*shadow_, puffTexture) || !BuildLayer_(*body_, puffTexture) || !BuildLayer_(*glow_, puffTexture)) {
    return false;
  }

  built_ = true;
  return true;
}

void StageNameClouds::Break(const Matrix4x4 &frame) {
  if (!built_ || broken_) {
    return;
  }
  broken_ = true;
  breakElapsed_ = 0.0f;

  // 霧: パフ 1 つにつき、柔らかい粒を mistWispsPerPuff 個、パフの位置から出す。散り散りに見えるよう、粒ごとに、次をばらつかせる。
  //   - 速さ: 衝撃点からの距離に比例する基本の速さ(衝撃点を中心に、全体が外へ広がる)に、粒ごとの倍率を掛ける
  //   - 向き: 外へ向かう向きに、衝撃点のまわりを回る向き(渦。回る向きも粒ごとにランダム)と、ランダムなずれを足す
  //   - カメラ側(板の前の逆向き = -z)へ寄る速さと、少し上へ浮く速さ
  //   - 大きさ(最初と消える頃)と、寿命
  // 2 個目以降の粒は、小さく、速くして、細かくちぎれた雲にする
  std::mt19937 random(kRandomSeed + 1u);
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
  const auto jitter = [&]() { return (unit(random) - 0.5f) * 2.0f * settings_.mistJitter; };

  for (const Puff &puff : puffs_) {
    const Vector3 position = TransformPoint({puff.target.x, puff.target.y, 0.0f}, frame);
    const Vector2 fromImpact{puff.target.x - impact_.x, puff.target.y - impact_.y};

    for (int wisp = 0; wisp < settings_.mistWispsPerPuff; ++wisp) {
      const bool isFragment = wisp > 0; // 2 個目以降: 細かくちぎれた雲
      const float speedScale = lerp(settings_.mistSpeedScaleMin, settings_.mistSpeedScaleMax, unit(random)) * (isFragment ? kFragmentSpeedRatio : 1.0f);
      const float swirl = settings_.mistSwirl * (unit(random) - 0.5f) * 2.0f; // 回る向きと強さ(-mistSwirl 〜 +mistSwirl)

      // 衝撃点からの向き(外向き)と、それに垂直な向き(回る向き。外向きを 90 度回したもの)
      const Vector3 localVelocity{(fromImpact.x * settings_.mistExpandSpeed - fromImpact.y * swirl) * speedScale + jitter(),
                                  (fromImpact.y * settings_.mistExpandSpeed + fromImpact.x * swirl) * speedScale + settings_.mistRise + jitter(),
                                  -lerp(settings_.mistToCameraMin, settings_.mistToCameraMax, unit(random))};
      const Vector3 velocity = TransformNormal(localVelocity, frame);

      const float startScale = puff.radius * lerp(settings_.mistStartScaleMin, settings_.mistStartScaleMax, unit(random)) *
                               (isFragment ? kFragmentScaleRatio : 1.0f);
      const float endScaleValue = startScale * lerp(settings_.mistEndScaleMin, settings_.mistEndScaleMax, unit(random));
      const Vector3 endScale{endScaleValue, endScaleValue, 1.0f};
      const float lifetime = lerp(settings_.mistLifetimeMin, settings_.mistLifetimeMax, unit(random));
      ParticleManager::GetInstance()->Emit(kStageCloudMistGroup, position, velocity, {startScale, startScale, 1.0f}, {0.0f, 0.0f, 0.0f},
                                           lifetime, {1.0f, 1.0f, 1.0f, settings_.mistAlpha}, &endScale);
    }
  }
}

void StageNameClouds::Update(float deltaTime) {
  time_ += deltaTime;
  if (broken_) {
    breakElapsed_ += deltaTime;
  }
}

void StageNameClouds::Draw(Renderer *renderer, const Matrix4x4 &frame) {
  if (!built_ || (broken_ && breakElapsed_ >= settings_.burstSeconds)) {
    return;
  }

  // 描画するカメラの右・上・前(ビュー行列の列。四角形を、このカメラへ向ける)
  const Matrix4x4 &view = renderer->GetViewMatrix();
  const Vector3 camRight{view.m[0][0], view.m[1][0], view.m[2][0]};
  const Vector3 camUp{view.m[0][1], view.m[1][1], view.m[2][1]};
  const Vector3 camForward{view.m[0][2], view.m[1][2], view.m[2][2]};

  // 壊れの進行度(0 〜 1。パフ本体が、膨らんで消える間)と、出現の進行度から、光の筋の位置(文字の左端 = 0、右端 = 1 を超えて進む)
  const float burst = broken_ ? Clamp01(breakElapsed_ / settings_.burstSeconds) : 0.0f;
  const float sweep = reveal_ * (1.0f + settings_.bandWidth);
  const bool finished = reveal_ >= 1.0f;

  auto fillQuad = [](Layer &layer, size_t index, const Vector3 &center, const Vector3 &right, const Vector3 &up) {
    VertexData *v = &layer.vertices[index * 4];
    // 左下、右下、右上、左上(BuildLayer_ の画像の座標と同じ順)
    const Vector3 corners[4] = {{center.x - right.x - up.x, center.y - right.y - up.y, center.z - right.z - up.z},
                                {center.x + right.x - up.x, center.y + right.y - up.y, center.z + right.z - up.z},
                                {center.x + right.x + up.x, center.y + right.y + up.y, center.z + right.z + up.z},
                                {center.x - right.x + up.x, center.y - right.y + up.y, center.z - right.z + up.z}};
    for (int i = 0; i < 4; ++i) {
      v[i].position = {corners[i].x, corners[i].y, corners[i].z, 1.0f};
    }
  };
  auto scaled = [](const Vector3 &axis, float length) { return Vector3{axis.x * length, axis.y * length, axis.z * length}; };

  for (size_t i = 0; i < puffs_.size(); ++i) {
    const Puff &puff = puffs_[i];

    // 光の筋が通ってからの育ち具合(0 = まだ、1 = 育ち切った)。筋が通る前は、パフは見えない
    const float k = finished ? 1.0f : Clamp01((sweep - puff.xNorm) / settings_.growSpan);
    float size = EaseOutBack(k);

    // 文字の中の位置: 育つにつれて、ずれた位置から、本来の位置へ集まる。組み上がったあとは、ごくわずかに揺れる
    const float settle = 1.0f - EaseOut(k);
    const float wobble = k >= 1.0f ? kWobbleAmplitude : 0.0f;
    Vector3 local{puff.target.x + puff.startOffset.x * settle + std::sin(time_ * kWobbleSpeed + puff.phase) * wobble,
                  puff.target.y + puff.startOffset.y * settle + std::cos(time_ * kWobbleSpeed + puff.phase) * wobble,
                  puff.startOffset.z * settle};

    // 突き破られた瞬間: パフ本体は、位置はそのままで、少し膨らむ(すぐ消える。広がる霧は、Break で出したパーティクルが担当する)
    if (broken_) {
      size *= 1.0f + burst * settings_.burstGrow;
    }

    // 光の筋の近くほど強く光る(出現の間だけ。筋の幅の中心が、先端より少し後ろ)
    float glow = 0.0f;
    if (!finished && !broken_ && k > 0.0f && settings_.bandWidth > 0.0f) {
      const float half = settings_.bandWidth * 0.5f;
      glow = Clamp01(1.0f - std::abs(sweep - half - puff.xNorm) / half);
    }

    const Vector3 center = TransformPoint(local, frame);
    const float bodyRadius = puff.radius * kQuadScale * size;

    // 影: 本体より、少し右下(画面の)・奥へずらす
    const float shiftRight = puff.radius * kShadowShiftRight * size;
    const float shiftDown = puff.radius * kShadowShiftDown * size;
    const Vector3 shadowCenter{center.x + camRight.x * shiftRight - camUp.x * shiftDown + camForward.x * kLayerDepthGap,
                               center.y + camRight.y * shiftRight - camUp.y * shiftDown + camForward.y * kLayerDepthGap,
                               center.z + camRight.z * shiftRight - camUp.z * shiftDown + camForward.z * kLayerDepthGap};
    fillQuad(*shadow_, i, shadowCenter, scaled(camRight, bodyRadius), scaled(camUp, bodyRadius));

    // 本体
    fillQuad(*body_, i, center, scaled(camRight, bodyRadius * kBodyScale), scaled(camUp, bodyRadius * kBodyScale));

    // 光: 手前へずらす。筋から離れたパフは、大きさ 0(見えない)
    const Vector3 glowCenter{center.x - camForward.x * kLayerDepthGap, center.y - camForward.y * kLayerDepthGap,
                             center.z - camForward.z * kLayerDepthGap};
    const float glowRadius = bodyRadius * kGlowScale * glow;
    fillQuad(*glow_, i, glowCenter, scaled(camRight, glowRadius), scaled(camUp, glowRadius));
  }

  // 頂点を書き換えて、影 -> 本体 -> 光の順に描く(深度を書き込む。雲は、深度を見て後から描かれるので、書かないと重なってしまう)
  const float fade = broken_ ? 1.0f - burst : 1.0f;
  const float shadowFade = fade;
  const float glowAlpha = broken_ ? 0.0f : kGlowColor.w;

  const auto drawLayer = [&](Layer &layer, const Vector4 &color, float alpha) {
    if (alpha <= 0.0f) {
      return;
    }
    layer.resource->UpdateVertices(layer.vertices.data(), static_cast<uint32_t>(layer.vertices.size()));
    layer.instance.SetColor({color.x, color.y, color.z, alpha});
    renderer->DrawEffectModel(&layer.instance, true);
  };
  drawLayer(*shadow_, kShadowColor, shadowFade);
  drawLayer(*body_, kBodyColor, fade);
  drawLayer(*glow_, kGlowColor, glowAlpha);
}
