#include "BossDefeatVisual.h"

#include <algorithm>

#include "AbsoluteEngine/scene/DissolveComponent.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/graphics/3d/model/ModelResource.h"
#include "AbsoluteEngine/graphics/3d/model/SkinCluster.h"
#include "AbsoluteEngine/graphics/Renderer.h"
#include "AbsoluteEngine/graphics/particle/GPUParticleManager.h"

namespace {
// ディゾルブのしきい値の上限。マスクの最大値(1)以上にして、外殻のすべての画素を消す
constexpr float kThresholdAllGone = 1.0f;

// 0 〜 1 のなめらかな補間(始まりと終わりがゆるやか)
float Smooth01(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}
} // namespace

void BossDefeatVisual::Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, std::shared_ptr<AbsoluteEngine::GameObject> shards,
                             const BossShardInfo &shardInfo, float crackEndThreshold, const BossCrackShape &crackShape,
                             const BossDefeatSettings &settings) {
  shell_ = std::move(shell);
  crackEnd_ = std::clamp(crackEndThreshold, 0.0f, kThresholdAllGone);
  crackShape_ = crackShape;
  lastCellBreak_ = 0.0f;
  settings_ = settings;

  // 欠片があれば、外殻を消して、欠片へ差し替える(割る前の外殻と、組み立てた欠片は、同じ形に見える)。
  // 欠片の準備に失敗したときは、shatter_ が有効にならないので、外殻のディゾルブで消える
  shatter_ = BossShatter();
  if (shards) {
    shatter_.Begin(std::move(shards), shardInfo, settings_.shatter);
  }
  if (shell_) {
    ApplyShell_(false, 0.0f); // 欠片へ差し替えているときは、ここで、外殻が全て消える
    ApplyCrack_(0.0f);
  }

  // 光線(ボロノイのひびを使うときだけ。ひびの線の上の頂点を GPU に選ばせるので、ひびと同じ形・太さを渡す)
  beams_ = BossLightBeams();
  if (UsesVoronoiCrack_()) {
    beams_.Begin(shell_, crackShape_, settings_.crackLineWidth, settings_.beams);
    explosion_.Begin(shell_, crackShape_, settings_.explosion);
  }
}

void BossDefeatVisual::Initialize() {
  explosion_.Initialize(BossExplosionDefaults::kModelPath);
}

void BossDefeatVisual::Update(ClearPhase phase, float phaseProgress, float deltaTime) {
  if (!shell_) return;
  const float p = std::clamp(phaseProgress, 0.0f, 1.0f);

  // 欠片(あるときだけ): 外殻の消去は、ApplyShell_ が行う。欠片の動きは、ここで進める
  shatter_.Update(phase, p, deltaTime);

  // セルが消えた割合(ボロノイのひびのボスだけが使う。Peel で 0 から 1 へ進み、それ以降は 1)
  float cellBreak = 0.0f;

  switch (phase) {
  case ClearPhase::Idle:
  case ClearPhase::HitStop:
    ApplyShell_(false, 0.0f);
    ApplyCrack_(0.0f);
    break;
  case ClearPhase::Crack:
    if (UsesVoronoiCrack_()) {
      // ひびの段階(ボロノイ): 外殻は消さずに、表面のひびが、広がり始める位置から外へ広がる
      ApplyShell_(false, 0.0f);
      ApplyCrack_(Smooth01(p));
    } else {
      // ひびの段階(マスク画像): 溝が開く。ひびの段階のないボス(crackEnd_ == 0)は、ここでは変化しない
      ApplyShell_(true, crackEnd_ * Smooth01(p));
    }
    break;
  case ClearPhase::Peel:
    if (UsesVoronoiCrack_()) {
      // 剥離の段階(ボロノイ): セルが、広がり始める位置に近いほうから、光って消えていく。外殻のディゾルブは使わない
      ApplyShell_(false, 0.0f);
      cellBreak = p;
      ApplyCrack_(1.0f, cellBreak);
    } else {
      // 剥離の段階(マスク画像): ひびの段階の最後から、すべて消えるまで
      ApplyShell_(true, Lerp(crackEnd_, kThresholdAllGone, p));
    }
    break;
  case ClearPhase::Burst:
    ApplyShell_(true, kThresholdAllGone);
    cellBreak = 1.0f; // セルも、すべて消えている
    ApplyCrack_(1.0f, cellBreak);
    break;
  case ClearPhase::Flow:
    ApplyShell_(true, kThresholdAllGone);
    cellBreak = 1.0f;
    ApplyCrack_(1.0f, cellBreak);
    break;
  case ClearPhase::Cover:
    ApplyShell_(true, kThresholdAllGone);
    cellBreak = 1.0f;
    ApplyCrack_(1.0f, cellBreak);
    break;
  }

  // 光線を伸ばす(ボロノイのひびを使わないときは、Begin していないので、何もしない)
  beams_.Update(phase, p);
  explosion_.Update(phase, p);

  // 前のフレームから今のフレームの間に消えたセルの頂点から、粒を出す。
  // 割合が進んだフレームだけ出し、進んだ分(previous から current)を、必ず 1 回ずつ通す(飛ばしても、取りこぼさない)
  if (UsesVoronoiCrack_() && cellBreak > lastCellBreak_) {
    EmitCellDust_(lastCellBreak_, cellBreak);
    lastCellBreak_ = cellBreak;
  }
}

void BossDefeatVisual::Draw() {
  beams_.Draw();
  explosion_.Draw(Renderer::GetInstance());
}

void BossDefeatVisual::ApplyShell_(bool enable, float threshold) {
  auto *dissolve = shell_->GetComponent<AbsoluteEngine::DissolveComponent>();
  if (!dissolve) return;
  // 欠片へ差し替えているときは、外殻をすべて消す(代わりに、欠片が描かれる)
  if (shatter_.IsActive()) {
    enable = true;
    threshold = kThresholdAllGone;
  }
  dissolve->enable = enable;
  dissolve->threshold = threshold;
  dissolve->edgeRange = settings_.edgeRange;
  dissolve->edgeColor = settings_.edgeColor;
  dissolve->emberAmount = settings_.emberAmount;
}

bool BossDefeatVisual::UsesVoronoiCrack_() const {
  return crackShape_.cellSize > 0.0f && !shatter_.IsActive();
}

void BossDefeatVisual::ApplyCrack_(float reveal, float breakAmount) {
  auto *dissolve = shell_->GetComponent<AbsoluteEngine::DissolveComponent>();
  if (!dissolve) return;
  ModelInstance::CrackParam &crack = dissolve->crack;
  crack.enable = UsesVoronoiCrack_();
  crack.cellSize = crackShape_.cellSize;
  crack.lineWidth = settings_.crackLineWidth;
  crack.reveal = reveal;
  crack.color = settings_.crackColor;
  crack.intensity = settings_.crackIntensity;
  crack.origin = {crackShape_.originX, crackShape_.originY, crackShape_.originZ};
  crack.spreadRadius = crackShape_.spreadRadius;
  crack.breakAmount = breakAmount;
}

void BossDefeatVisual::EmitCellDust_(float previousBreak, float currentBreak) {
  auto *modelComponent = shell_->GetComponent<AbsoluteEngine::ModelComponent>();
  if (!modelComponent) return;
  ModelInstance *instance = modelComponent->GetModelInstance();
  if (!instance) return;
  ModelResource *resource = instance->GetResource();
  SkinCluster *skinCluster = instance->GetSkinCluster();
  // 骨のないモデルは、スキニングの入力が無いので、粒を出さない
  if (!resource || !skinCluster || !resource->HasBones()) return;

  GPUParticleManager::SurfaceEmitDesc desc;
  desc.vertexSrvIndex = resource->GetVertexSRVIndex();
  desc.influenceSrvIndex = resource->GetBoneSRVIndex();
  desc.paletteSrvIndex = skinCluster->srvIndex;
  desc.vertexCount = resource->GetVertexCount();
  desc.world = modelComponent->GetWorldMatrix();
  // ひびの形は、外殻の描画(ApplyCrack_)と同じ値にする。セルが消える場所と、粒が出る場所を、そろえるため
  desc.crackOrigin = {crackShape_.originX, crackShape_.originY, crackShape_.originZ};
  desc.crackCellSize = crackShape_.cellSize;
  desc.crackSpreadRadius = crackShape_.spreadRadius;
  desc.prevBreak = previousBreak;
  desc.curBreak = currentBreak;
  desc.emitChance = settings_.dustEmitChance;
  desc.outwardSpeed = settings_.dustOutwardSpeed;

  GPUParticleManager::ParticleStyle &style = desc.style;
  style.colorStart = settings_.dustColorStart;
  style.colorEnd = settings_.dustColorEnd;
  style.coolRatio = settings_.dustCoolRatio;
  style.fadePower = settings_.dustFadePower;
  style.lifeMin = settings_.dustLifeMin;
  style.lifeMax = settings_.dustLifeMax;
  style.sizeMin = settings_.dustSizeMin;
  style.sizeMax = settings_.dustSizeMax;
  style.speedMin = settings_.dustSpeedMin;
  style.speedMax = settings_.dustSpeedMax;

  GPUParticleManager::GetInstance()->RequestSurfaceEmit(desc);
}
