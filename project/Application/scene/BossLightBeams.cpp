#include "BossLightBeams.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/graphics/3d/model/CrackPointPicker.h"
#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/graphics/3d/model/ModelResource.h"
#include "AbsoluteEngine/graphics/3d/model/ModelUtils.h"
#include "AbsoluteEngine/graphics/3d/model/SkinCluster.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "Logger.h"
#include "Method.h"

namespace {
// 向きを持たないとみなす、ベクトルの長さの下限
constexpr float kMinDirectionLength = 0.001f;
// 長さのない光線は描かない(これ以下は、まだ伸び始めていないか、縮み切ったものとして扱う)
constexpr float kMinBeamLength = 0.01f;
// 縮む時間の幅(セルが光り始めてから消えるまで)の下限。0 での除算を避ける
constexpr float kMinShrinkSpan = 1.0e-4f;
// 根元が体の中心に一致して、向きが決まらないときの向き(真上)
const Vector3 kFallbackDirection{0.0f, 1.0f, 0.0f};

float Saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

Vector3 PlusV(const Vector3 &a, const Vector3 &b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 MinusV(const Vector3 &a, const Vector3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 TimesV(const Vector3 &v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}
} // namespace

void BossLightBeams::Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, const BossCrackShape &crackShape, float crackLineWidth,
                           const BossLightBeamSettings &settings) {
  shell_ = std::move(shell);
  crackShape_ = crackShape;
  settings_ = settings;
  beams_.clear();
  pickRequested_ = false;
  built_ = false;
  random_.seed(std::random_device{}());

  // ひびの形がない(従来のディゾルブで消える)ボスは、光線を出さない
  if (!shell_ || crackShape_.cellSize <= 0.0f) {
    shell_.reset();
    return;
  }

  // 骨(スキニング)付きのモデルだけが、光線の根元を、動きに合わせて動かせる
  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  ModelInstance *instance = modelComponent ? modelComponent->GetModelInstance() : nullptr;
  ModelResource *resource = instance ? instance->GetResource() : nullptr;
  if (!resource || !instance->GetSkinCluster() || !resource->HasBones()) {
    shell_.reset();
    return;
  }

  // ひびの線の上の頂点を、GPU に選ばせる(ひびが光る場所と一致する)。結果は、次のフレーム以降に Update が受け取る
  CrackPointPicker::PickDesc desc;
  desc.vertexSrvIndex = resource->GetVertexSRVIndex();
  desc.vertexCount = resource->GetVertexCount();
  desc.cellSize = crackShape_.cellSize;
  desc.maxEdge = crackLineWidth;
  desc.chance = settings_.pickChance;
  desc.seed = std::uniform_real_distribution<float>(0.0f, BossLightBeamDefaults::kPickSeedRange)(random_);
  // セルが消える時刻の計算に使う、ひび割れの形(外殻の描画と同じ値)
  desc.originX = crackShape_.originX;
  desc.originY = crackShape_.originY;
  desc.originZ = crackShape_.originZ;
  desc.spreadRadius = crackShape_.spreadRadius;
  pickRequested_ = CrackPointPicker::GetInstance()->Request(desc);
  if (!pickRequested_) {
    // 依頼できなかった(未初期化など)。光線なしで、演出を続ける
    shell_.reset();
  }
}

float BossLightBeams::ToTimeline_(ClearPhase phase, float progress) const {
  switch (phase) {
  case ClearPhase::Idle:
  case ClearPhase::HitStop:
    return 0.0f;
  case ClearPhase::Crack:
    return Saturate(progress); // Crack の間に、光線が 1 本ずつ増えて、出そろう
  default:
    return 1.0f; // Peel 以降(出そろっている)
  }
}

float BossLightBeams::ToBreakAmount_(ClearPhase phase, float progress) const {
  switch (phase) {
  case ClearPhase::Idle:
  case ClearPhase::HitStop:
  case ClearPhase::Crack:
    return 0.0f;
  case ClearPhase::Peel:
    return Saturate(progress);
  default:
    return 1.0f; // Burst 以降(セルは、すべて消えている)
  }
}

void BossLightBeams::Update(ClearPhase phase, float phaseProgress) {
  if (!shell_) return;

  // 候補が読めるまで待つ(依頼した次のフレーム以降)。読めたら、光線の一覧を作る
  if (!built_) {
    std::vector<CrackPointPicker::PickedPoint> candidates;
    if (!CrackPointPicker::GetInstance()->TryCollect(candidates)) return;
    BuildBeams_(candidates);
    built_ = true;
  }

  const float timeline = ToTimeline_(phase, phaseProgress);
  const float breakAmount = ToBreakAmount_(phase, phaseProgress);
  for (Beam &beam : beams_) {
    UpdateBeam_(beam, timeline, breakAmount);
  }
}

void BossLightBeams::BuildBeams_(const std::vector<CrackPointPicker::PickedPoint> &candidates) {
  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  ModelInstance *instance = modelComponent ? modelComponent->GetModelInstance() : nullptr;
  ModelResource *resource = instance ? instance->GetResource() : nullptr;
  if (!resource || !resource->GetModelData()) return;

  // CPU 側の頂点の位置と、骨の影響(GPU に渡したものと同じ並び)
  const std::vector<Vector3> positions = resource->GetVertexPositions();
  const std::vector<VertexBoneData> boneData = FlattenSkinningData(*resource->GetModelData());
  if (positions.empty() || positions.size() != boneData.size()) return;

  // 候補の、ひびの広がり始める位置からの向き(モデル空間)。長さがほぼ 0 の頂点は、向きが決まらないので使わない
  const Vector3 origin{crackShape_.originX, crackShape_.originY, crackShape_.originZ};
  std::vector<CrackPointPicker::PickedPoint> usable;
  std::vector<Vector3> directions;
  for (const CrackPointPicker::PickedPoint &candidate : candidates) {
    if (candidate.vertexIndex >= positions.size()) continue;
    const Vector3 offset = MinusV(positions[candidate.vertexIndex], origin);
    if (Length(offset) < kMinDirectionLength) continue;
    usable.push_back(candidate);
    directions.push_back(Normalize(offset));
  }
  // 調整の手がかり: GPU が選んだ候補の数(ひびの線の上の頂点 x pickChance)と、使える数
  Logger::GetInstance().Log("[BossLightBeams] picked candidates=%d usable=%d beams<=%d", static_cast<int>(candidates.size()),
                            static_cast<int>(usable.size()), static_cast<int>(settings_.beamCount));
  if (usable.empty()) return;

  // 選び方: 「消える時刻が、Peel の間に均等にばらける」ことと、「向きがばらける」ことの、両方を満たす。
  //   消える時刻: 候補の、セルが消える時刻の範囲を、光線の数の区間に等分し、区間ごとに 1 本ずつ選ぶ。
  //     (体の中心に近いセルは早く消えるので、何も考えずに選ぶと、前半に偏り、光線が崩壊の序盤にまとめて消えてしまう)
  //   向き: 区間の中では、すでに選んだ向きのどれからも、いちばん離れている候補を選ぶ(最も遠い点から順に選ぶ方法)。放射状に広がる。
  // 区間を選ぶ順は、乱数で入れ替える(伸びる順と、消える順を、そろえない)
  const size_t count = (std::min)(static_cast<size_t>(settings_.beamCount), usable.size());
  float vanishMin = usable.front().vanishTime;
  float vanishMax = usable.front().vanishTime;
  for (const CrackPointPicker::PickedPoint &point : usable) {
    vanishMin = (std::min)(vanishMin, point.vanishTime);
    vanishMax = (std::max)(vanishMax, point.vanishTime);
  }
  const float vanishSpan = (std::max)(vanishMax - vanishMin, kMinShrinkSpan);

  std::vector<size_t> bucketOrder(count);
  for (size_t b = 0; b < count; ++b) bucketOrder[b] = b;
  std::shuffle(bucketOrder.begin(), bucketOrder.end(), random_);

  std::vector<size_t> chosen;
  chosen.reserve(count);
  std::vector<bool> used(usable.size(), false);
  for (size_t bucket : bucketOrder) {
    const float lo = vanishMin + vanishSpan * static_cast<float>(bucket) / static_cast<float>(count);
    const float hi = vanishMin + vanishSpan * static_cast<float>(bucket + 1) / static_cast<float>(count);
    const bool lastBucket = bucket + 1 == count; // 最後の区間は、上端(最大の時刻)も含める

    // 区間の中で、すでに選んだ向きから、いちばん離れている候補(最初の 1 本は、区間の中で、乱数で選ぶ)
    size_t best = usable.size();
    float bestScore = 3.0f; // 選んだ向きとの内積の最大値(小さいほど離れている)
    for (size_t i = 0; i < usable.size(); ++i) {
      if (used[i]) continue;
      const float v = usable[i].vanishTime;
      if (v < lo || (lastBucket ? v > hi : v >= hi)) continue;
      float score = std::uniform_real_distribution<float>(-1.0f, 1.0f)(random_);
      if (!chosen.empty()) {
        score = -2.0f;
        for (size_t c : chosen) score = (std::max)(score, Dot(directions[i], directions[c]));
      }
      if (score < bestScore) {
        bestScore = score;
        best = i;
      }
    }
    if (best == usable.size()) {
      // この区間に候補が無い。区間の中央に、時刻がいちばん近い、未使用の候補を選ぶ
      float nearest = 1.0e9f;
      const float middle = 0.5f * (lo + hi);
      for (size_t i = 0; i < usable.size(); ++i) {
        if (used[i]) continue;
        const float distance = std::fabs(usable[i].vanishTime - middle);
        if (distance < nearest) {
          nearest = distance;
          best = i;
        }
      }
    }
    if (best == usable.size()) break; // 候補を使い切った
    used[best] = true;
    chosen.push_back(best);
  }

  // 光線の一覧を作る。選んだ順に、伸び始める時刻を割り当てる(選んだ順は、向きが離れているので、続けて伸びる光線が離れる)
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const size_t beamCount = chosen.size(); // 候補を使い切ったときは、count より少なくなる
  beams_.assign(beamCount, Beam{});
  for (size_t k = 0; k < beamCount; ++k) {
    Beam &beam = beams_[k];
    const CrackPointPicker::PickedPoint &picked = usable[chosen[k]];
    const uint32_t index = picked.vertexIndex;
    beam.vertexIndex = index;
    // 光線の通る点のセルが、光り始めて消える時刻(GPU が、描画と同じ計算で出したもの)。この間に、光線も縮んで消える
    beam.heatStart = picked.heatStart;
    beam.vanishAt = picked.vanishTime;
    beam.bindPosition = positions[index];
    for (int j = 0; j < 4; ++j) {
      beam.boneIDs[j] = boneData[index].boneIDs[j];
      beam.weights[j] = boneData[index].weights[j];
    }
    const float t = beamCount > 1 ? static_cast<float>(k) / static_cast<float>(beamCount - 1) : 0.0f;
    beam.startAt = settings_.firstStart + (settings_.lastStart - settings_.firstStart) * std::pow(t, settings_.startCurve);
    beam.length = settings_.maxLength * (1.0f - settings_.lengthJitter * unit(random_));
    beam.width = settings_.width * (1.0f - settings_.widthJitter * unit(random_));

    // 調整の手がかり: 光線ごとの、セルが光り始める時刻と消える時刻(Peel の長さを 1 とした 0 〜 1)
    Logger::GetInstance().Log("[BossLightBeams] beam %d: vertex=%u startAt=%.2f heatStart=%.3f vanishAt=%.3f", static_cast<int>(k), index,
                              beam.startAt, beam.heatStart, beam.vanishAt);

    RibbonTrail::Params params;
    params.lifetime = BossLightBeamDefaults::kRibbonLifetime;
    params.width = beam.width;
    params.tailWidthRatio = 0.0f; // 先が尖る
    params.color = settings_.color;
    beam.trail.Initialize(params);
  }
}

bool BossLightBeams::SkinnedWorldPosition_(const Beam &beam, Vector3 &out) const {
  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  ModelInstance *instance = modelComponent ? modelComponent->GetModelInstance() : nullptr;
  SkinCluster *skin = instance ? instance->GetSkinCluster() : nullptr;
  Skeleton *skeleton = instance ? instance->GetSkeleton() : nullptr;
  if (!skin || !skin->mappedPalette || !skeleton) return false;

  // GPU のスキニング(Skinning.CS)と同じ計算: 骨ごとの変換を、重みで混ぜる(どれも位置の一次結合なので、行列を混ぜても同じ)
  Vector3 skinned{0.0f, 0.0f, 0.0f};
  for (int j = 0; j < 4; ++j) {
    if (beam.weights[j] <= 0.0f || beam.boneIDs[j] >= skeleton->joints.size()) continue;
    const Vector3 moved = TransformPoint(beam.bindPosition, skin->mappedPalette[beam.boneIDs[j]].skeletonSpaceMatrix);
    skinned = PlusV(skinned, TimesV(moved, beam.weights[j]));
  }
  out = TransformPoint(skinned, modelComponent->GetWorldMatrix());
  return true;
}

void BossLightBeams::UpdateBeam_(Beam &beam, float timeline, float breakAmount) {
  beam.visible = false;

  // 伸び具合: 勢いよく伸びて、伸び切る手前でゆるやかに止まる(1 - (1 - g)^3)
  const float grown = Saturate((timeline - beam.startAt) / settings_.growSpan);
  const float eased = 1.0f - std::pow(1.0f - grown, 3.0f);
  // 縮み具合: 通る点のセルが光り始める時刻(heatStart)から、消える時刻(vanishAt)にかけて、1 から 0 へ縮む(先端から根元へ)。
  // セルが消えた瞬間に、光線も消える。Crack までは、breakAmount が 0 なので、縮まない。Burst 以降は 1 で、縮み切っている
  const float shrinkSpan = (std::max)(beam.vanishAt - beam.heatStart, kMinShrinkSpan);
  const float remain = 1.0f - Saturate((breakAmount - beam.heatStart) / shrinkSpan);
  const float length = beam.length * eased * remain;
  if (length < kMinBeamLength) return;

  Vector3 root;
  if (!SkinnedWorldPosition_(beam, root)) return;

  // 向き: ひびの広がり始める位置(体の中心。ワールド)から、選んだ頂点へ向かう向き(放射状)
  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  const Vector3 origin = TransformPoint({crackShape_.originX, crackShape_.originY, crackShape_.originZ}, modelComponent->GetWorldMatrix());
  Vector3 direction = MinusV(root, origin);
  direction = Length(direction) < kMinDirectionLength ? kFallbackDirection : Normalize(direction);

  // 帯の通過点(細く薄い側から): 先端、中間、根元。中間点までは太さと明るさを保ち、先端で急に尖る(槍のような形)。
  // 根元は、体の中心に置く(選んだ頂点ではない)。体の内側の部分は、殻に隠れ、ひびの線の上の点から、光が突き出して見える
  const Vector3 tip = PlusV(origin, TimesV(direction, length));
  const Vector3 middle = PlusV(origin, TimesV(direction, length * settings_.midPosition));
  const float lifetime = beam.trail.GetLifetime();
  const std::vector<RibbonTrail::Point> path = {
      {tip, lifetime},
      {middle, lifetime * settings_.midAgeRatio},
      {origin, 0.0f},
  };
  beam.trail.SetPath(path);
  beam.trail.SetWidth(beam.width);
  beam.trail.SetColor(settings_.color);
  beam.visible = true;
}

void BossLightBeams::Draw() {
  if (!shell_) return;
  for (const Beam &beam : beams_) {
    if (!beam.visible) continue;
    // 通常のアルファ合成で描く(加算だと、明るい空や雲の上で、光が白く飛んで見えなくなる)
    Renderer::GetInstance()->DrawRibbon(beam.trail, BlendMode::Alpha);
  }
}
