#include "BossShatter.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/graphics/3d/model/ModelUtils.h"
#include "AbsoluteEngine/graphics/3d/model/SkinCluster.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "Logger.h"
#include "Method.h"

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 2.0f * kPi;
// 長さがこれ以下のベクトルは、向きが決められないので、正規化しない
constexpr float kMinDirectionLength = 1e-4f;
// 体積の下限(体積が 0 の欠片が、重心の計算で無視されすぎないように)
constexpr float kMinVolume = 1e-4f;

float Smooth01(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

Vector3 Plus(const Vector3 &a, const Vector3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Minus(const Vector3 &a, const Vector3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 Times(const Vector3 &v, float s) { return {v.x * s, v.y * s, v.z * s}; }

// 長さ 1 に直す。長さがほぼ 0 のときは fallback を返す
Vector3 NormalizedOr(const Vector3 &v, const Vector3 &fallback) {
  const float length = Length(v);
  return length > kMinDirectionLength ? Times(v, 1.0f / length) : fallback;
}

// 軸(長さ 1)まわりに angle だけ回すクォータニオン
Quaternion QuatFromAxisAngle(const Vector3 &axis, float angle) {
  const float half = angle * 0.5f;
  const float s = std::sin(half);
  return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
}

// クォータニオンの積(a の回転のあとに b の回転を重ねる)。結果は長さ 1 に直す
Quaternion QuatMultiply(const Quaternion &a, const Quaternion &b) {
  Quaternion r{
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
  };
  const float length = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
  if (length > kMinDirectionLength) {
    const float inv = 1.0f / length;
    r = {r.x * inv, r.y * inv, r.z * inv, r.w * inv};
  }
  return r;
}

// 乱数の生成器から、範囲の一様乱数を引く
float RandomRange(std::mt19937 &rng, float minValue, float maxValue) {
  return std::uniform_real_distribution<float>(minValue, maxValue)(rng);
}

// 球の表面の、一様な向き(長さ 1)
Vector3 RandomDirection(std::mt19937 &rng) {
  const float z = RandomRange(rng, -1.0f, 1.0f);
  const float phi = RandomRange(rng, 0.0f, kTwoPi);
  const float r = std::sqrt((std::max)(0.0f, 1.0f - z * z));
  return {r * std::cos(phi), r * std::sin(phi), z};
}
} // namespace

bool BossShatter::Begin(std::shared_ptr<AbsoluteEngine::GameObject> shardsObject, const BossShardInfo &info,
                        const BossShatterSettings &settings) {
  shards_.clear();
  object_.reset();
  settings_ = settings;
  time_ = 0.0f;
  if (!shardsObject) return false;

  auto *model = shardsObject->GetComponent<AbsoluteEngine::ModelComponent>();
  ModelInstance *instance = model ? model->GetModelInstance() : nullptr;
  Skeleton *skeleton = instance ? instance->GetSkeleton() : nullptr;
  if (!skeleton || !instance->GetSkinCluster()) {
    Logger::GetInstance().Log("[BossShatter] the shards model has no skeleton (bones are required)");
    return false;
  }

  // 基準ポーズの骨格空間の行列を求める(各欠片の中心と、親の行列を取り出すため)
  UpdateSkeleton(*skeleton);
  const Matrix4x4 world = model->GetWorldMatrix();
  invWorld_ = Inverse(world);

  // 各ジョイントの親を求める(子の一覧から逆引きする)
  std::vector<int32_t> parentOf(skeleton->joints.size(), -1);
  for (const Joint &joint : skeleton->joints) {
    for (int32_t childIndex : joint.children) {
      if (childIndex >= 0 && static_cast<size_t>(childIndex) < parentOf.size()) parentOf[childIndex] = joint.index;
    }
  }

  // 付属データの欠片のうち、骨が見つかったものだけを動かす
  std::vector<Vector3> worldCenters;
  std::vector<float> volumes;
  for (const BossShardEntry &entry : info.GetEntries()) {
    const auto it = skeleton->jointMap.find(entry.name);
    if (it == skeleton->jointMap.end()) {
      Logger::GetInstance().Log("[BossShatter] bone not found for shard: %s", entry.name.c_str());
      continue;
    }
    const Joint &joint = skeleton->joints[it->second];

    Shard shard;
    shard.jointIndex = it->second;
    shard.restTranslate = joint.transform.translate;
    shard.restRotate = joint.transform.rotate;
    shard.restScale = joint.transform.scale;
    const int32_t parent = parentOf[it->second];
    shard.parentInverse = parent >= 0 ? Inverse(skeleton->joints[parent].skeletonSpaceMatrix) : MakeIdentity4x4();

    // 欠片の中心 = 骨の原点(骨格空間)。ワールドへ直す
    const Vector3 center{joint.skeletonSpaceMatrix.m[3][0], joint.skeletonSpaceMatrix.m[3][1], joint.skeletonSpaceMatrix.m[3][2]};
    worldCenters.push_back(TransformPoint(center, world));
    volumes.push_back((std::max)(entry.volume, kMinVolume));
    shards_.push_back(shard);
  }
  if (shards_.empty()) {
    Logger::GetInstance().Log("[BossShatter] no shard could be matched to a bone");
    return false;
  }

  // コアの中心 = 全欠片の、体積で重みをつけた重心
  Vector3 coreCenter{0.0f, 0.0f, 0.0f};
  float totalVolume = 0.0f;
  for (size_t i = 0; i < shards_.size(); ++i) {
    coreCenter = Plus(coreCenter, Times(worldCenters[i], volumes[i]));
    totalVolume += volumes[i];
  }
  coreCenter = Times(coreCenter, 1.0f / totalVolume);

  // コアからの向きと距離の順位。順位は、最も遠い欠片を 1 として割る
  float maxDistance = 0.0f;
  std::vector<float> distances(shards_.size());
  for (size_t i = 0; i < shards_.size(); ++i) {
    distances[i] = Length(Minus(worldCenters[i], coreCenter));
    maxDistance = (std::max)(maxDistance, distances[i]);
  }

  // 欠片ごとのばらつき(固定の種なので、毎回同じ割れ方になる)
  std::mt19937 rng(BossShatterDefaults::kRandomSeed);
  for (size_t i = 0; i < shards_.size(); ++i) {
    Shard &shard = shards_[i];
    shard.direction = NormalizedOr(Minus(worldCenters[i], coreCenter), {0.0f, 1.0f, 0.0f});
    shard.rank = maxDistance > kMinDirectionLength ? distances[i] / maxDistance : 0.0f;
    // 外側の欠片(rank が大きい)ほど、早く解放する
    shard.releaseAt = std::clamp((1.0f - shard.rank) * settings_.releaseSpread + RandomRange(rng, 0.0f, settings_.releaseJitter), 0.0f, 1.0f);
    shard.launchSpeed = RandomRange(rng, settings_.launchSpeedMin, settings_.launchSpeedMax);
    shard.shakeAxis = RandomDirection(rng);
    shard.shakePhase = RandomRange(rng, 0.0f, kTwoPi);
    shard.angularAxis = RandomDirection(rng);
    shard.angularSpeed = RandomRange(rng, settings_.angularSpeedMin, settings_.angularSpeedMax);
  }

  object_ = std::move(shardsObject);
  // 組み立てた状態の骨を、すぐ書き込む(最初の描画で、スキニングの行列が未設定のままにならないように)
  WritePose_();
  return true;
}

void BossShatter::Update(ClearPhase phase, float phaseProgress, float deltaTime) {
  if (!IsActive()) return;
  const float progress = std::clamp(phaseProgress, 0.0f, 1.0f);
  time_ += deltaTime;

  // 破裂(Burst)以降は、まだ解放していない欠片をすべて解放する(スキップで、Peel を飛ばしたときも揃うように)
  const bool releaseAllNow = phase == ClearPhase::Burst || phase == ClearPhase::Flow || phase == ClearPhase::Cover;
  const float scale = ScaleFor_(phase, progress);

  for (Shard &shard : shards_) {
    if (!shard.released) {
      if (releaseAllNow) {
        Release_(shard, settings_.burstSpeedBoost);
      } else if (phase == ClearPhase::Peel && progress >= shard.releaseAt) {
        Release_(shard, 1.0f);
      }
    }

    if (shard.released) {
      Integrate_(shard, deltaTime);
    } else {
      shard.offset = HeldOffset_(shard, phase, progress);
    }
    shard.scale = scale;
  }
  WritePose_();
}

void BossShatter::Release_(Shard &shard, float speedBoost) const {
  shard.released = true;
  // 外へ向かう速さと、舞い上がる速さを足す。開いていた継ぎ目の位置(offset)からそのまま飛び出す
  shard.velocity = Plus(Times(shard.direction, shard.launchSpeed * speedBoost), {0.0f, settings_.upwardBias, 0.0f});
}

void BossShatter::Integrate_(Shard &shard, float deltaTime) const {
  shard.velocity.y -= settings_.gravity * deltaTime;
  // 速度に比例した減速(フレームレートに依存しない、指数的な減衰)
  shard.velocity = Times(shard.velocity, std::exp(-settings_.dragPerSecond * deltaTime));
  shard.offset = Plus(shard.offset, Times(shard.velocity, deltaTime));
  shard.rotation = QuatMultiply(QuatFromAxisAngle(shard.angularAxis, shard.angularSpeed * deltaTime), shard.rotation);
}

Vector3 BossShatter::HeldOffset_(const Shard &shard, ClearPhase phase, float progress) const {
  // 継ぎ目の開き(0 〜 1)と、震えの強さ(0 〜 1)
  float open = 0.0f;
  float shakeEnvelope = 0.0f;
  if (phase == ClearPhase::Crack) {
    // コアに近い欠片から順に開く。外側の欠片は、遅れて開き始める
    const float delay = shard.rank * settings_.crackDelaySpread;
    open = Smooth01((progress - delay) / (std::max)(1.0f - delay, kMinDirectionLength));
    shakeEnvelope = progress;
  } else if (phase == ClearPhase::Peel) {
    // 解放されるまで、開いたまま震え続ける
    open = 1.0f;
    shakeEnvelope = 1.0f;
  }
  const Vector3 gap = Times(shard.direction, settings_.gapDistance * open);
  const float shake = std::sin(time_ * settings_.shakeFrequency + shard.shakePhase) * settings_.shakeAmplitude * shakeEnvelope;
  return Plus(gap, Times(shard.shakeAxis, shake));
}

float BossShatter::ScaleFor_(ClearPhase phase, float progress) const {
  if (phase == ClearPhase::Flow) {
    return (std::max)(1.0f - std::pow(progress, settings_.shrinkPower), BossShatterDefaults::kMinScale);
  }
  if (phase == ClearPhase::Cover) return BossShatterDefaults::kMinScale;
  return 1.0f;
}

void BossShatter::WritePose_() {
  if (!object_) return;
  auto *model = object_->GetComponent<AbsoluteEngine::ModelComponent>();
  ModelInstance *instance = model ? model->GetModelInstance() : nullptr;
  Skeleton *skeleton = instance ? instance->GetSkeleton() : nullptr;
  SkinCluster *skinCluster = instance ? instance->GetSkinCluster() : nullptr;
  if (!skeleton || !skinCluster) return;

  for (const Shard &shard : shards_) {
    Joint &joint = skeleton->joints[shard.jointIndex];
    // ワールドの移動量 -> モデル空間 -> 骨の親のローカル、の順に直す(行ベクトル規約なので、右から行列を掛ける)
    const Vector3 localOffset = TransformNormal(TransformNormal(shard.offset, invWorld_), shard.parentInverse);
    joint.transform.translate = Plus(shard.restTranslate, localOffset);
    joint.transform.rotate = QuatMultiply(shard.rotation, shard.restRotate);
    joint.transform.scale = {shard.restScale.x * shard.scale, shard.restScale.y * shard.scale, shard.restScale.z * shard.scale};
  }

  UpdateSkeleton(*skeleton);
  skinCluster->Update(*skeleton);
}
