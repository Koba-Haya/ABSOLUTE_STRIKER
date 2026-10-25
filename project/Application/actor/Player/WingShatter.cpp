#include "WingShatter.h"

#include "AbsoluteEngine/scene/ModelComponent.h"
#include "ModelResource.h"
#include "Method.h"
#include "Renderer.h"
#include "TextureResource.h"
#include "../../scene/PlayerDeathDirector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <unordered_map>

namespace {
// アトラス(翅の表と裏を並べたテクスチャ)の中で、表と裏を分ける UV の x(左半分 = 表、右半分 = 裏。wing_shape.py の配置)
constexpr float kAtlasSurfaceSplit = 0.5f;
// 表と裏のペアを探すための、UV の量子化の細かさ(1 を何等分するか)
constexpr float kPairQuantization = 8192.0f;
constexpr int kPairKeyBits = 20;
// 長さがこれ以下のベクトルは、向きが決められないので、正規化しない
constexpr float kMinDirectionLength = 1e-4f;

float VectorLength(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vector3 Sub(const Vector3& a, const Vector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

float DistanceSquared(const Vector3& a, const Vector3& b) {
  const Vector3 d = Sub(a, b);
  return d.x * d.x + d.y * d.y + d.z * d.z;
}

Vector3 Normalized(const Vector3& v, const Vector3& fallback) {
  const float length = VectorLength(v);
  return length > kMinDirectionLength ? Vector3{v.x / length, v.y / length, v.z / length} : fallback;
}

Vector3 PositionOf(const VertexData& v) { return {v.position.x, v.position.y, v.position.z}; }

// 表と裏のペアを探すための鍵(左右、UV の x(表と裏でそろえたもの)、UV の y を量子化して 1 つの数にする)
uint64_t MakePairKey(bool leftSide, float surfaceU, float v) {
  const uint64_t su = static_cast<uint64_t>(std::lround(surfaceU * kPairQuantization));
  const uint64_t sv = static_cast<uint64_t>(std::lround(v * kPairQuantization));
  return (static_cast<uint64_t>(leftSide ? 1 : 0) << (kPairKeyBits * 2)) | (su << kPairKeyBits) | sv;
}
} // namespace

WingShatter::WingShatter() = default;
WingShatter::~WingShatter() = default;

bool WingShatter::Initialize(AbsoluteEngine::ModelComponent* playerModel, const std::shared_ptr<TextureResource>& atlasTexture,
                             const std::shared_ptr<TextureResource>& dissolveMask, const WingShatterSettings& settings) {
  built_ = false;
  releasedCount_ = 0;
  releasesThisFrame_.clear();
  clusters_.clear();
  settings_ = settings;
  playerModel_ = playerModel;
  modelResourceRaw_ = nullptr;
  modelData_ = nullptr;
  if (!playerModel || !atlasTexture) return false;

  ModelInstance* instance = playerModel->GetModelInstance();
  if (!instance || !instance->GetSkeleton()) return false;
  ModelResource* resource = instance->GetResource();
  if (!resource || !resource->GetModelData()) return false;
  modelData_ = resource->GetModelData();

  // 翅のメッシュ = 頂点が最も多いメッシュ(蝶は、翅 2856 頂点、胴 1596 頂点)
  if (modelData_->meshes.size() < 2) return false;
  size_t best = 0;
  for (size_t i = 1; i < modelData_->meshes.size(); ++i) {
    if (modelData_->meshes[i].vertices.size() > modelData_->meshes[best].vertices.size()) best = i;
  }
  wingMeshIndex_ = static_cast<uint32_t>(best);
  wingVertexOffset_ = 0;
  wingIndexStart_ = 0;
  for (size_t i = 0; i < best; ++i) {
    wingVertexOffset_ += static_cast<uint32_t>(modelData_->meshes[i].vertices.size());
    wingIndexStart_ += static_cast<uint32_t>(modelData_->meshes[i].indices.size());
  }
  const MeshData& wing = modelData_->meshes[best];
  if (wing.skinningData.size() != wing.vertices.size() || wing.indices.size() < 3) return false;
  triangleCount_ = static_cast<uint32_t>(wing.indices.size() / 3);

  originalIndices_ = FlattenIndices(*modelData_);
  currentIndices_ = originalIndices_;
  modelResourceRaw_ = resource;
  resource->UpdateIndices(originalIndices_.data(), static_cast<uint32_t>(originalIndices_.size())); // 前の実行の名残を戻す
  indicesDirty_ = false;

  BuildClusters_();

  // 破片の描画用のモデル: 全かたまりの三角形を、頂点を共有しない形で並べる(三角形 1 つにつき 3 頂点)
  auto shardData = std::make_shared<ModelData>();
  MeshData shardMesh;
  shardMesh.vertices.assign(static_cast<size_t>(triangleCount_) * 3, VertexData{});
  for (VertexData& v : shardMesh.vertices) v.position = {0.0f, 0.0f, 0.0f, 1.0f};
  shardMesh.indices.resize(shardMesh.vertices.size());
  for (uint32_t i = 0; i < shardMesh.indices.size(); ++i) shardMesh.indices[i] = i;
  worldVertices_ = shardMesh.vertices;
  localVertices_ = shardMesh.vertices;
  shardData->meshes.push_back(std::move(shardMesh));

  shardResource_ = std::make_shared<ModelResource>();
  ModelResource::CreateInfo createInfo{};
  createInfo.dx = Renderer::GetInstance()->GetDX();
  createInfo.modelData = shardData;
  createInfo.texture = atlasTexture;
  if (!shardResource_->Initialize(createInfo)) return false;

  ModelInstance::CreateInfo instanceInfo{};
  instanceInfo.resource = shardResource_;
  instanceInfo.lightingMode = ModelLightingMode::kThinSurface;
  if (!shardInstance_.Initialize(instanceInfo)) return false;
  shardInstance_.SetWorld(MakeIdentity4x4());
  if (dissolveMask) shardInstance_.SetDissolveMaskTexture(dissolveMask);

  built_ = true;
  return true;
}

void WingShatter::BuildClusters_() {
  const MeshData& wing = modelData_->meshes[wingMeshIndex_];

  // --- 1. 表と裏のペアを作る(UV が同じ位置の三角形どうし) ---
  struct TriInfo {
    Vector3 centroid{0.0f, 0.0f, 0.0f}; // 初期姿勢(バインド)での中心
    bool isTop = true;
    bool leftSide = false;
    float surfaceU = 0.0f;
    float v = 0.0f;
  };
  std::vector<TriInfo> info(triangleCount_);
  for (uint32_t t = 0; t < triangleCount_; ++t) {
    Vector3 c{0.0f, 0.0f, 0.0f};
    Vector2 uv{0.0f, 0.0f};
    for (int k = 0; k < 3; ++k) {
      const VertexData& vd = wing.vertices[wing.indices[t * 3 + k]];
      c.x += vd.position.x; c.y += vd.position.y; c.z += vd.position.z;
      uv.x += vd.texcoord.x; uv.y += vd.texcoord.y;
    }
    constexpr float kThird = 1.0f / 3.0f;
    info[t].centroid = {c.x * kThird, c.y * kThird, c.z * kThird};
    uv = {uv.x * kThird, uv.y * kThird};
    info[t].isTop = uv.x < kAtlasSurfaceSplit;
    info[t].leftSide = info[t].centroid.x < 0.0f;
    info[t].surfaceU = info[t].isTop ? uv.x : uv.x - kAtlasSurfaceSplit;
    info[t].v = uv.y;
  }

  std::vector<TrianglePair> pairs;
  std::unordered_map<uint64_t, uint32_t> pairOfKey;
  for (uint32_t t = 0; t < triangleCount_; ++t) {
    if (!info[t].isTop) continue;
    TrianglePair pair;
    pair.triangles[0] = t;
    pair.count = 1;
    pairOfKey[MakePairKey(info[t].leftSide, info[t].surfaceU, info[t].v)] = static_cast<uint32_t>(pairs.size());
    pairs.push_back(pair);
  }
  for (uint32_t t = 0; t < triangleCount_; ++t) {
    if (info[t].isTop) continue;
    auto it = pairOfKey.find(MakePairKey(info[t].leftSide, info[t].surfaceU, info[t].v));
    if (it != pairOfKey.end() && pairs[it->second].count == 1) {
      pairs[it->second].triangles[1] = t;
      pairs[it->second].count = 2;
    } else {
      TrianglePair single; // 相手が見つからない裏の三角形は、単独で扱う
      single.triangles[0] = t;
      single.count = 1;
      pairs.push_back(single);
    }
  }

  // --- 2. 左右それぞれで、種(かたまりの中心)を、離れた位置に選ぶ(最も遠い点を、順に足していく) ---
  std::mt19937 random(settings_.randomSeed);
  std::uniform_real_distribution<float> signed01(-1.0f, 1.0f);
  std::vector<std::vector<uint32_t>> pairsOfSide(2);
  for (uint32_t i = 0; i < pairs.size(); ++i) {
    pairsOfSide[info[pairs[i].triangles[0]].leftSide ? 1 : 0].push_back(i);
  }

  std::vector<uint32_t> clusterOfPair(pairs.size(), 0);
  for (int side = 0; side < 2; ++side) {
    const std::vector<uint32_t>& list = pairsOfSide[side];
    if (list.empty()) continue;
    const size_t seedCount = (std::min<size_t>)(static_cast<size_t>((std::max)(1, settings_.clustersPerSide)), list.size());

    // 最初の種: 翼幅方向(x)に最も外側のペア
    std::vector<uint32_t> seeds;
    uint32_t first = list[0];
    for (uint32_t p : list) {
      if (std::fabs(info[pairs[p].triangles[0]].centroid.x) > std::fabs(info[pairs[first].triangles[0]].centroid.x)) first = p;
    }
    seeds.push_back(first);
    std::vector<float> nearest(list.size(), 1e30f);
    while (seeds.size() < seedCount) {
      const Vector3 last = info[pairs[seeds.back()].triangles[0]].centroid;
      uint32_t farthest = list[0];
      float farthestDistance = -1.0f;
      for (size_t i = 0; i < list.size(); ++i) {
        const float d = DistanceSquared(info[pairs[list[i]].triangles[0]].centroid, last);
        nearest[i] = (std::min)(nearest[i], d);
        if (nearest[i] > farthestDistance) {
          farthestDistance = nearest[i];
          farthest = list[i];
        }
      }
      seeds.push_back(farthest);
    }

    // 各ペアを、最も近い種のかたまりへ
    const uint32_t clusterBase = static_cast<uint32_t>(clusters_.size());
    clusters_.resize(clusters_.size() + seeds.size());
    for (uint32_t p : list) {
      const Vector3 c = info[pairs[p].triangles[0]].centroid;
      size_t bestSeed = 0;
      float bestDistance = 1e30f;
      for (size_t s = 0; s < seeds.size(); ++s) {
        const float d = DistanceSquared(c, info[pairs[seeds[s]].triangles[0]].centroid);
        if (d < bestDistance) {
          bestDistance = d;
          bestSeed = s;
        }
      }
      clusterOfPair[p] = clusterBase + static_cast<uint32_t>(bestSeed);
    }
  }
  for (uint32_t p = 0; p < pairs.size(); ++p) {
    Cluster& cluster = clusters_[clusterOfPair[p]];
    for (uint32_t k = 0; k < pairs[p].count; ++k) cluster.triangles.push_back(pairs[p].triangles[k]);
  }
  // 三角形のないかたまり(種が重なった場合など)は取り除く
  clusters_.erase(std::remove_if(clusters_.begin(), clusters_.end(), [](const Cluster& c) { return c.triangles.empty(); }), clusters_.end());

  // --- 3. 中心と、破片の頂点バッファの中の位置、解放の時刻(外側ほど早い)を決める ---
  uint32_t vertexCursor = 0;
  for (Cluster& cluster : clusters_) {
    Vector3 sum{0.0f, 0.0f, 0.0f};
    for (uint32_t t : cluster.triangles) {
      sum.x += info[t].centroid.x; sum.y += info[t].centroid.y; sum.z += info[t].centroid.z;
    }
    const float inv = 1.0f / static_cast<float>(cluster.triangles.size());
    cluster.bindCentroid = {sum.x * inv, sum.y * inv, sum.z * inv};
    cluster.firstVertex = vertexCursor;
    vertexCursor += static_cast<uint32_t>(cluster.triangles.size()) * 3;
  }
  std::vector<size_t> order(clusters_.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return std::fabs(clusters_[a].bindCentroid.x) > std::fabs(clusters_[b].bindCentroid.x);
  });
  for (size_t rank = 0; rank < order.size(); ++rank) {
    const float ratio = order.size() > 1 ? static_cast<float>(rank) / static_cast<float>(order.size() - 1) : 0.0f;
    const float progress = settings_.releaseStartProgress + (settings_.releaseEndProgress - settings_.releaseStartProgress) * ratio +
                           settings_.releaseJitter * signed01(random);
    clusters_[order[rank]].releaseProgress = std::clamp(progress, 0.0f, 1.0f);
  }
}

void WingShatter::Reset() {
  releasedCount_ = 0;
  releasesThisFrame_.clear();
  for (Cluster& cluster : clusters_) cluster.released = false;
  if (modelResourceRaw_ && !originalIndices_.empty()) {
    currentIndices_ = originalIndices_;
    modelResourceRaw_->UpdateIndices(originalIndices_.data(), static_cast<uint32_t>(originalIndices_.size()));
  }
  indicesDirty_ = false;
  if (shardResource_) {
    for (VertexData& v : worldVertices_) v.position = {0.0f, 0.0f, 0.0f, 1.0f};
    shardResource_->UpdateVertices(worldVertices_.data(), static_cast<uint32_t>(worldVertices_.size()));
  }
}

void WingShatter::SkinVertex_(uint32_t vertex, Vector3& position, Vector3& normal) const {
  const MeshData& wing = modelData_->meshes[wingMeshIndex_];
  const VertexData& vd = wing.vertices[vertex];
  const VertexBoneData& bone = wing.skinningData[vertex];
  const Skeleton* skeleton = playerModel_->GetModelInstance()->GetSkeleton();

  // GPU のスキニング(Skinning.CS.hlsl)と同じ式: ボーンごとの行列(逆バインド × スケルトン空間)の、重み付き和
  Matrix4x4 skin{};
  for (int k = 0; k < 4; ++k) {
    if (bone.weights[k] <= 0.0f || bone.boneIDs[k] >= skeleton->joints.size()) continue;
    const Joint& joint = skeleton->joints[bone.boneIDs[k]];
    const Matrix4x4 palette = Multiply(joint.inverseBindPoseMatrix, joint.skeletonSpaceMatrix);
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) skin.m[r][c] += bone.weights[k] * palette.m[r][c];
    }
  }
  const Matrix4x4 toWorld = Multiply(skin, playerModel_->GetWorldMatrix());
  position = TransformPoint(PositionOf(vd), toWorld);
  normal = Normalized(TransformNormal(vd.normal, toWorld), {0.0f, 1.0f, 0.0f});
}

void WingShatter::Release_(Cluster& cluster, const Vector3& bodyVelocity) {
  const MeshData& wing = modelData_->meshes[wingMeshIndex_];

  // いまのポーズのワールド座標と法線を求め、中心を決める
  std::vector<Vector3> positions(cluster.triangles.size() * 3);
  std::vector<Vector3> normals(cluster.triangles.size() * 3);
  Vector3 sum{0.0f, 0.0f, 0.0f};
  for (size_t j = 0; j < cluster.triangles.size(); ++j) {
    for (int k = 0; k < 3; ++k) {
      SkinVertex_(wing.indices[cluster.triangles[j] * 3 + k], positions[j * 3 + k], normals[j * 3 + k]);
      sum.x += positions[j * 3 + k].x; sum.y += positions[j * 3 + k].y; sum.z += positions[j * 3 + k].z;
    }
  }
  const float inv = 1.0f / static_cast<float>(positions.size());
  cluster.position = {sum.x * inv, sum.y * inv, sum.z * inv};

  // 中心からの相対の頂点(回転は、この上に掛ける)
  for (size_t j = 0; j < cluster.triangles.size(); ++j) {
    for (int k = 0; k < 3; ++k) {
      const VertexData& source = wing.vertices[wing.indices[cluster.triangles[j] * 3 + k]];
      VertexData& local = localVertices_[cluster.firstVertex + j * 3 + k];
      const Vector3 offset = Sub(positions[j * 3 + k], cluster.position);
      local.position = {offset.x, offset.y, offset.z, 1.0f};
      local.normal = normals[j * 3 + k];
      local.texcoord = source.texcoord;
    }
  }

  // 初速: 蝶の速度 + 蝶の中心から外へ + 上へ。大きさと回転は、かたまりごとにばらつかせる
  std::mt19937 random(settings_.randomSeed ^ (cluster.firstVertex * 2654435761u));
  std::uniform_real_distribution<float> signed01(-1.0f, 1.0f);
  const Matrix4x4 world = playerModel_->GetWorldMatrix();
  const Vector3 bodyCenter{world.m[3][0], world.m[3][1], world.m[3][2]};
  const Vector3 outward = Normalized(Sub(cluster.position, bodyCenter), {1.0f, 0.0f, 0.0f});
  const float jitter = 1.0f + settings_.speedJitter * signed01(random);
  cluster.velocity = {bodyVelocity.x * settings_.inheritVelocityScale + outward.x * settings_.outwardSpeed * jitter,
                      bodyVelocity.y * settings_.inheritVelocityScale + outward.y * settings_.outwardSpeed * jitter + settings_.upwardSpeed * jitter,
                      bodyVelocity.z * settings_.inheritVelocityScale + outward.z * settings_.outwardSpeed * jitter};
  cluster.angle = {0.0f, 0.0f, 0.0f};
  cluster.angularVelocity = {settings_.spinSpeed * signed01(random), settings_.spinSpeed * signed01(random), settings_.spinSpeed * signed01(random)};
  cluster.released = true;
  ++releasedCount_;

  // 翅の描画から消す(三角形の 3 つの番号を、同じ頂点にして、面積 0 にする)
  for (uint32_t t : cluster.triangles) {
    const uint32_t base = wingIndexStart_ + t * 3;
    currentIndices_[base + 1] = currentIndices_[base];
    currentIndices_[base + 2] = currentIndices_[base];
  }
  indicesDirty_ = true;

  WingShardRelease release;
  release.position = cluster.position;
  release.direction = Normalized(cluster.velocity, outward);
  release.triangleCount = static_cast<uint32_t>(cluster.triangles.size());
  releasesThisFrame_.push_back(release);
}

void WingShatter::Update(float deltaTime, float crumbleProgress, const Vector3& bodyVelocity) {
  releasesThisFrame_.clear();
  if (!built_) return;

  // 崩壊の進み具合に達したかたまりを、解放する
  if (crumbleProgress > 0.0f) {
    for (Cluster& cluster : clusters_) {
      if (!cluster.released && crumbleProgress >= cluster.releaseProgress) {
        Release_(cluster, bodyVelocity);
      }
    }
  }
  if (indicesDirty_) {
    modelResourceRaw_->UpdateIndices(currentIndices_.data(), static_cast<uint32_t>(currentIndices_.size()));
    indicesDirty_ = false;
  }
  if (releasedCount_ == 0) return;

  // 解放したかたまりを動かし、ワールドの頂点を作る
  const float drag = (std::max)(0.0f, 1.0f - settings_.dragPerSecond * deltaTime);
  for (Cluster& cluster : clusters_) {
    if (!cluster.released) continue;
    cluster.velocity.y -= settings_.gravity * deltaTime;
    cluster.velocity = {cluster.velocity.x * drag, cluster.velocity.y * drag, cluster.velocity.z * drag};
    cluster.position = {cluster.position.x + cluster.velocity.x * deltaTime, cluster.position.y + cluster.velocity.y * deltaTime,
                        cluster.position.z + cluster.velocity.z * deltaTime};
    cluster.angle = {cluster.angle.x + cluster.angularVelocity.x * deltaTime, cluster.angle.y + cluster.angularVelocity.y * deltaTime,
                     cluster.angle.z + cluster.angularVelocity.z * deltaTime};

    const Matrix4x4 rotation = Multiply(Multiply(MakeRotateXMatrix(cluster.angle.x), MakeRotateYMatrix(cluster.angle.y)),
                                        MakeRotateZMatrix(cluster.angle.z));
    const uint32_t vertexCount = static_cast<uint32_t>(cluster.triangles.size()) * 3;
    for (uint32_t i = 0; i < vertexCount; ++i) {
      const VertexData& local = localVertices_[cluster.firstVertex + i];
      VertexData& world = worldVertices_[cluster.firstVertex + i];
      const Vector3 offset = TransformNormal(PositionOf(local), rotation); // 回転だけ(移動は含めない)
      world.position = {cluster.position.x + offset.x, cluster.position.y + offset.y, cluster.position.z + offset.z, 1.0f};
      world.normal = TransformNormal(local.normal, rotation);
      world.texcoord = local.texcoord;
    }
  }
  shardResource_->UpdateVertices(worldVertices_.data(), static_cast<uint32_t>(worldVertices_.size()));
}

void WingShatter::Draw(const PlayerDeathDissolve& dissolve) {
  if (!built_ || releasedCount_ == 0) return;
  shardInstance_.SetDissolveParam(dissolve.enabled, dissolve.threshold, dissolve.edgeRange, dissolve.edgeColor, {1.0f, 1.0f, 1.0f});
  shardInstance_.Draw();
}
