#include "StageNameFragments.h"

#include "ModelResource.h"
#include "ModelUtils.h"
#include "Renderer.h"
#include "TextureResource.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>

namespace {
constexpr float kTwoPi = 6.2831853f;
// ライティングモード 0: ライトの影響を受けず、テクスチャの色をそのまま出す(板と同じ)
constexpr int32_t kUnlitMode = 0;
// 長さがこれ以下のベクトルは、向きが決められないので、正規化しない
constexpr float kMinDirectionLength = 1e-4f;
// 画像の座標(UV)は、上が 0、下が 1(板の y は上が正なので、反転する)
constexpr float kUvTop = 0.0f;
constexpr float kUvBottom = 1.0f;

VertexData MakeVertex(const Vector3 &position, const Vector2 &uv, const Vector3 &normal) {
  VertexData v;
  v.position = {position.x, position.y, position.z, 1.0f};
  v.texcoord = uv;
  v.normal = normal;
  return v;
}

// 多角形の頂点の平均(破片の中心に使う。凸多角形なら、必ず多角形の中に入る)
Vector2 AverageOf(const std::vector<Vector2> &polygon) {
  Vector2 sum{0.0f, 0.0f};
  for (const Vector2 &p : polygon) {
    sum.x += p.x;
    sum.y += p.y;
  }
  const float inv = 1.0f / static_cast<float>(polygon.size());
  return {sum.x * inv, sum.y * inv};
}
} // namespace

StageNameFragments::StageNameFragments() = default;
StageNameFragments::~StageNameFragments() = default;

bool StageNameFragments::Build(const std::vector<VoronoiCell> &cells, const FractureRect &rect, float worldScale,
                               const std::shared_ptr<TextureResource> &texture, const FragmentSettings &settings) {
  built_ = false;
  launched_ = false;
  settings_ = settings;
  worldScale_ = worldScale;
  fragments_.clear();
  localVertices_.clear();
  if (cells.empty() || !texture) {
    return false;
  }

  const float halfThickness = settings.thickness * 0.5f;
  const float rectWidth = rect.max.x - rect.min.x;
  const float rectHeight = rect.max.y - rect.min.y;
  // 板の中の座標から、画像の座標(UV)へ。x は左が 0、y は上が 0(板の y は上が正なので反転する)
  auto toUv = [&](const Vector2 &p) -> Vector2 {
    return {(p.x - rect.min.x) / rectWidth, kUvTop + (kUvBottom - kUvTop) * (1.0f - (p.y - rect.min.y) / rectHeight)};
  };

  auto meshData = std::make_shared<ModelData>();
  MeshData mesh;

  for (const VoronoiCell &cell : cells) {
    const std::vector<Vector2> &polygon = cell.polygon;
    const size_t count = polygon.size();

    Fragment fragment;
    const Vector2 center = AverageOf(polygon);
    fragment.centroid = {center.x * worldScale, center.y * worldScale, 0.0f};
    fragment.firstVertex = static_cast<uint32_t>(mesh.vertices.size());

    // 破片の中心からの相対の位置(前面は手前 = -z、背面は奥 = +z)
    auto localPosition = [&](const Vector2 &p, float z) -> Vector3 {
      return {p.x * worldScale - fragment.centroid.x, p.y * worldScale - fragment.centroid.y, z};
    };

    const uint32_t base = fragment.firstVertex;
    // 前面と背面の頂点(多角形の頂点と同じ順。前面は base + i、背面は base + count + i)
    for (size_t i = 0; i < count; ++i) {
      mesh.vertices.push_back(MakeVertex(localPosition(polygon[i], -halfThickness), toUv(polygon[i]), {0.0f, 0.0f, -1.0f}));
    }
    for (size_t i = 0; i < count; ++i) {
      mesh.vertices.push_back(MakeVertex(localPosition(polygon[i], halfThickness), toUv(polygon[i]), {0.0f, 0.0f, 1.0f}));
    }
    // 前面・背面の三角形(頂点 0 を中心にした扇形。凸多角形なので、これで覆える)
    for (uint32_t i = 1; i + 1 < count; ++i) {
      mesh.indices.insert(mesh.indices.end(), {base, base + i, base + i + 1});
      mesh.indices.insert(mesh.indices.end(), {base + static_cast<uint32_t>(count), base + static_cast<uint32_t>(count) + i + 1,
                                               base + static_cast<uint32_t>(count) + i});
    }
    // 側面: 辺ごとに四角形 1 枚。法線は、多角形の外向き(反時計回りの多角形で、辺の右側)。
    // 頂点を前面・背面と共有しないのは、側面の法線を面ごとにそろえる(角を立てる)ため
    for (size_t i = 0; i < count; ++i) {
      const Vector2 &a = polygon[i];
      const Vector2 &b = polygon[(i + 1) % count];
      Vector3 normal{b.y - a.y, -(b.x - a.x), 0.0f};
      const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y);
      if (length > kMinDirectionLength) {
        normal = {normal.x / length, normal.y / length, 0.0f};
      }
      const uint32_t side = static_cast<uint32_t>(mesh.vertices.size());
      mesh.vertices.push_back(MakeVertex(localPosition(a, -halfThickness), toUv(a), normal));
      mesh.vertices.push_back(MakeVertex(localPosition(b, -halfThickness), toUv(b), normal));
      mesh.vertices.push_back(MakeVertex(localPosition(a, halfThickness), toUv(a), normal));
      mesh.vertices.push_back(MakeVertex(localPosition(b, halfThickness), toUv(b), normal));
      mesh.indices.insert(mesh.indices.end(), {side, side + 1, side + 2, side + 2, side + 1, side + 3});
    }

    fragment.vertexCount = static_cast<uint32_t>(mesh.vertices.size()) - fragment.firstVertex;
    fragments_.push_back(fragment);
  }

  localVertices_ = mesh.vertices;
  worldVertices_ = mesh.vertices;
  meshData->meshes.push_back(std::move(mesh));

  // 1 つのモデル(全破片の頂点をまとめたもの)として、頂点バッファを作る
  resource_ = std::make_shared<ModelResource>();
  ModelResource::CreateInfo createInfo{};
  createInfo.dx = Renderer::GetInstance()->GetDX();
  createInfo.modelData = meshData;
  createInfo.texture = texture;
  if (!resource_->Initialize(createInfo)) {
    return false;
  }

  ModelInstance::CreateInfo instanceInfo{};
  instanceInfo.resource = resource_;
  instanceInfo.lightingMode = kUnlitMode;
  if (!instance_.Initialize(instanceInfo)) {
    return false;
  }

  built_ = true;
  return true;
}

void StageNameFragments::Launch(const Vector2 &impact) {
  if (!built_ || launched_) {
    return;
  }
  std::mt19937 random(settings_.randomSeed);
  std::uniform_real_distribution<float> signed01(-1.0f, 1.0f);
  std::uniform_real_distribution<float> unit01(0.0f, 1.0f);

  for (Fragment &fragment : fragments_) {
    // 衝撃点から破片の中心への、板の面に沿った向きと距離。
    // 破片の中心は、Build で worldScale 倍(ワールドの長さ)にしてあるので、衝撃点も同じ倍率に直して比べる
    Vector2 away{fragment.centroid.x - impact.x * worldScale_, fragment.centroid.y - impact.y * worldScale_};
    float distance = std::sqrt(away.x * away.x + away.y * away.y);
    if (distance > kMinDirectionLength) {
      away = {away.x / distance, away.y / distance};
    } else {
      // 真上にある破片は、向きが決まらないので、ランダムな向きにする
      const float angle = kTwoPi * unit01(random);
      away = {std::cos(angle), std::sin(angle)};
      distance = 0.0f;
    }

    // 衝撃点に近いほど 1 に近く、離れるほど 0 に近づく。blastHalfDistance 離れると 0.5
    const float nearness = settings_.blastHalfDistance / (settings_.blastHalfDistance + distance);
    const float jitter = 1.0f + settings_.speedJitter * signed01(random);

    fragment.velocity = {away.x * settings_.blastSpeed * nearness * jitter, away.y * settings_.blastSpeed * nearness * jitter,
                         settings_.forwardSpeed * (0.5f + 0.5f * nearness) * jitter};
    const float spin = settings_.spinSpeed * nearness;
    fragment.angularVelocity = {spin * signed01(random), spin * signed01(random), spin * signed01(random)};
  }

  elapsed_ = 0.0f;
  launched_ = true;
}

void StageNameFragments::Update(float deltaTime) {
  if (!launched_) {
    return;
  }
  elapsed_ += deltaTime;
  for (Fragment &fragment : fragments_) {
    fragment.velocity.y -= settings_.gravity * deltaTime;
    fragment.offset = {fragment.offset.x + fragment.velocity.x * deltaTime, fragment.offset.y + fragment.velocity.y * deltaTime,
                       fragment.offset.z + fragment.velocity.z * deltaTime};
    fragment.angle = {fragment.angle.x + fragment.angularVelocity.x * deltaTime, fragment.angle.y + fragment.angularVelocity.y * deltaTime,
                      fragment.angle.z + fragment.angularVelocity.z * deltaTime};
  }
}

void StageNameFragments::Draw(Renderer *renderer, const Matrix4x4 &frame) {
  if (!IsActive()) {
    return;
  }

  // 破片ごとに、「回転 -> 中心へ移動 -> 板の位置・向き」の順で変換し、ワールドの頂点を作る
  for (const Fragment &fragment : fragments_) {
    const Matrix4x4 rotation = Multiply(Multiply(MakeRotateXMatrix(fragment.angle.x), MakeRotateYMatrix(fragment.angle.y)),
                                        MakeRotateZMatrix(fragment.angle.z));
    const Vector3 center{fragment.centroid.x + fragment.offset.x, fragment.centroid.y + fragment.offset.y,
                         fragment.centroid.z + fragment.offset.z};
    const Matrix4x4 toWorld = Multiply(Multiply(rotation, MakeTranslateMatrix(center)), frame);
    const Matrix4x4 normalToWorld = Multiply(rotation, frame);

    for (uint32_t i = 0; i < fragment.vertexCount; ++i) {
      const VertexData &local = localVertices_[fragment.firstVertex + i];
      VertexData &world = worldVertices_[fragment.firstVertex + i];
      const Vector3 p = TransformPoint({local.position.x, local.position.y, local.position.z}, toWorld);
      world.position = {p.x, p.y, p.z, 1.0f};
      // 法線は向きだけを変える(移動は含めない)。回転と、拡大のない板の向きの行列なので、そのまま使える
      world.normal = TransformNormal(local.normal, normalToWorld);
    }
  }

  resource_->UpdateVertices(worldVertices_.data(), static_cast<uint32_t>(worldVertices_.size()));
  instance_.SetColor({1.0f, 1.0f, 1.0f, CalcAlpha_()});
  // 深度を書き込む(雲が、破片の手前にあるときだけ重なる)
  renderer->DrawEffectModel(&instance_, true);
}

bool StageNameFragments::IsActive() const {
  return built_ && launched_ && elapsed_ < settings_.lifetimeSeconds;
}

float StageNameFragments::CalcAlpha_() const {
  const float fadeStart = settings_.lifetimeSeconds - settings_.fadeSeconds;
  if (settings_.fadeSeconds <= 0.0f || elapsed_ <= fadeStart) {
    return 1.0f;
  }
  return std::clamp(1.0f - (elapsed_ - fadeStart) / settings_.fadeSeconds, 0.0f, 1.0f);
}
