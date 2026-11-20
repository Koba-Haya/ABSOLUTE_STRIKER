#include "NextIslandView.h"

#include <algorithm>
#include <cmath>

#include "AbsoluteEngine/resources/AssetManager.h"
#include "Logger.h"
#include "ModelResource.h"

namespace {
float Saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

// 0 〜 1 のなめらかな補間(始まりと終わりがゆるやか)
float Smooth01(float t) {
  t = Saturate(t);
  return t * t * (3.0f - 2.0f * t);
}
} // namespace

void NextIslandView::Initialize(const char *modelPath) {
  ready_ = false;
  auto resource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(modelPath);
  if (!resource) {
    Logger::GetInstance().Log("[NextIslandView] failed to load the island model: %s", modelPath);
    return;
  }
  ready_ = island_.Initialize(resource);
  pillar_.Initialize();
}

void NextIslandView::Begin(const Vector3 &bossCenter, const Vector3 &cameraEye, const Vector3 &cameraTarget,
                           const NextIslandSettings &settings) {
  settings_ = settings;
  scaleRatio_ = 0.0f;
  active_ = ready_;

  // 進行方向: レールカメラの向き(水平)。真上や真下を向いていて決められないときは、ワールドの前(+z)
  Vector3 forward{cameraTarget.x - cameraEye.x, 0.0f, cameraTarget.z - cameraEye.z};
  const float length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
  if (length > NextIslandDefaults::kMinForwardLength) {
    forward.x /= length;
    forward.z /= length;
  } else {
    forward = {0.0f, 0.0f, 1.0f};
  }
  // 右(左手系・y 上: 上 x 前 = 右。上 = (0, 1, 0) なので、右 = (前.z, 0, -前.x))
  const Vector3 right{forward.z, 0.0f, -forward.x};

  position_ = {bossCenter.x + forward.x * settings_.aheadDistance + right.x * settings_.lateralOffset,
               bossCenter.y + settings_.heightOffset,
               bossCenter.z + forward.z * settings_.aheadDistance + right.z * settings_.lateralOffset};
  targetPoint_ = {position_.x, position_.y + settings_.scale * NextIslandDefaults::kTopHeightPerScale, position_.z};
  viewCenter_ = {position_.x, position_.y - settings_.scale * NextIslandDefaults::kViewCenterDropPerScale, position_.z};
  boundRadius_ = settings_.scale * NextIslandDefaults::kBoundRadiusPerScale;
  reviveRatio_ = 0.0f;
  pillar_.Begin(position_); // 柱の根元は、島の上面の中心
}

void NextIslandView::Update(ClearPhase phase, float phaseProgress) {
  if (!active_) return;
  // Peel の間に育つ。Peel より前は 0(描かない)、Burst 以降は 1
  switch (phase) {
  case ClearPhase::Peel:
    scaleRatio_ = Smooth01(phaseProgress);
    break;
  case ClearPhase::Burst:
  case ClearPhase::Flow:
  case ClearPhase::Release:
  case ClearPhase::Cover:
    scaleRatio_ = 1.0f;
    break;
  default:
    scaleRatio_ = 0.0f;
    break;
  }

  // 蘇り: Release で、柱が立ち上がったあと(kRiseShare)、波面が島の表面を広がる。Cover(スキップを含む)では、蘇りきっている。それ以前は、くすんだまま
  if (phase == ClearPhase::Release) {
    const float riseShare = IslandReleasePillarDefaults::kRiseShare;
    reviveRatio_ = Smooth01((phaseProgress - riseShare) / (1.0f - riseShare));
  } else if (phase == ClearPhase::Cover) {
    reviveRatio_ = 1.0f;
  } else {
    reviveRatio_ = 0.0f;
  }
  pillar_.Update(phase, phaseProgress);
}

void NextIslandView::Draw(Renderer *renderer) {
  if (!ready_ || !active_ || !renderer || scaleRatio_ <= 0.0f) return;
  const float scale = settings_.scale * (std::max)(scaleRatio_, NextIslandDefaults::kMinScaleRatio);
  island_.SetPlacement(position_, scale, {0.0f, settings_.yaw, 0.0f});

  // 蘇りの見た目で描く(はじめは、くすんだ色。蘇りの中心は、島の上面の中心 = モデルの原点)
  ModelInstance::CrackParam revive;
  revive.enable = true;
  revive.cellSize = NextIslandDefaults::kReviveNoiseCell;
  revive.lineWidth = NextIslandDefaults::kReviveFrontWidth;
  revive.reveal = reviveRatio_;
  revive.color = NextIslandDefaults::kReviveGlowColor;
  revive.intensity = NextIslandDefaults::kReviveGlowIntensity;
  revive.origin = {0.0f, 0.0f, 0.0f};
  revive.spreadRadius = NextIslandDefaults::kReviveSpread;
  island_.DrawRevive(renderer, revive);
}

void NextIslandView::DrawEffects(Renderer *renderer) {
  if (!ready_ || !active_) return;
  pillar_.Draw(renderer);
}
