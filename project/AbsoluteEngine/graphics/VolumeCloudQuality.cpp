#include "VolumeCloudQuality.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

namespace {

// 品質の段ごとの「上限」。基準の設定がこれより重いときだけ、軽い方へ丸める(基準より重くはしない)。
// 段 0 は上限なし(基準そのまま)
struct TierCap {
  int32_t minDivisor;     // 解像度の分母の下限(大きいほど軽い)
  int32_t maxSteps;       // 視線のステップ数の上限
  int32_t maxLightSteps;  // 太陽方向のステップ数の上限
};
constexpr TierCap kTierCaps[VolumeCloudQuality::kTierCount] = {
    {kVolumeCloudMinResolutionDivisor, 64, 8}, // 0: 基準そのまま(上限は、シェーダーの最大値と同じ)
    {2, 24, 3},                                // 1
    {2, 16, 2},                                // 2
    {4, 16, 2},                                // 3: 最低。ステップを 16 より減らすと、ジッタの粒(網点のような模様)が目立つため、ここまでにする
};

constexpr float kSmoothingSec = 0.5f;      // 移動平均の時定数(短いほど素早く追う。長いほどなめらか)
constexpr float kResumeGapSec = 0.5f;      // 呼ばれない期間がこれより長ければ、再開とみなす
constexpr float kMaxDeltaSec = 0.1f;       // 1 回の更新で進める時間の上限(長い停止で、一気に判断しないため)
constexpr float kRegretWindowSec = 10.0f;  // 上げてからこの時間以内に下がったら、上げたのが早すぎたとみなす
constexpr float kMaxUpWaitSec = 60.0f;     // 上げるまでの待ち時間の上限

} // namespace

void VolumeCloudQuality::Update(float frameGpuMs, float deltaSec) {
  // 最初の呼び出し、または長い空白のあと: 判断を見送る時間からやり直す(段は変えない)
  if (!started_ || deltaSec > kResumeGapSec) {
    started_ = true;
    smoothedMs_ = frameGpuMs;
    overSec_ = 0.0f;
    underSec_ = 0.0f;
    sinceChangeSec_ = 0.0f;
    warmupLeftSec_ = config_.warmupSec;
    upWaitSec_ = config_.baseUpWaitSec;
    return;
  }
  deltaSec = (std::min)(deltaSec, kMaxDeltaSec);

  // 移動平均(指数): 新しい値へ、時定数に応じた割合だけ近づく
  const float alpha = 1.0f - std::exp(-deltaSec / kSmoothingSec);
  smoothedMs_ += (frameGpuMs - smoothedMs_) * alpha;

  if (!config_.enabled) {
    return;
  }
  warmupLeftSec_ -= deltaSec;
  if (warmupLeftSec_ > 0.0f) {
    return;
  }
  sinceChangeSec_ += deltaSec;

  // 重い(または軽い)状態が続いている時間を数える
  overSec_ = smoothedMs_ > config_.downMs ? overSec_ + deltaSec : 0.0f;
  underSec_ = smoothedMs_ < config_.upMs ? underSec_ + deltaSec : 0.0f;

  if (overSec_ >= config_.downHoldSec && tier_ < kTierCount - 1) {
    // 上げてすぐ下がったなら、上げるのが早すぎた。次に上げるまでの待ち時間を倍にして、上げ下げの往復を減らす
    if (lastChangeWasUp_ && sinceChangeSec_ < kRegretWindowSec) {
      upWaitSec_ = (std::min)(upWaitSec_ * 2.0f, kMaxUpWaitSec);
    }
    ChangeTier_(tier_ + 1);
    lastChangeWasUp_ = false;
  } else if (underSec_ >= config_.upHoldSec && tier_ > 0 && sinceChangeSec_ >= upWaitSec_) {
    ChangeTier_(tier_ - 1);
    lastChangeWasUp_ = true;
  }

  // 長く安定したら、伸ばした待ち時間を元へ戻す
  if (sinceChangeSec_ > upWaitSec_ * 2.0f && upWaitSec_ > config_.baseUpWaitSec) {
    upWaitSec_ = config_.baseUpWaitSec;
  }
}

void VolumeCloudQuality::ChangeTier_(int32_t newTier) {
  Logger::GetInstance().Log("[VolumeCloudQuality] tier %d -> %d (GPU frame %.1f ms)\n", tier_, newTier, smoothedMs_);
  tier_ = newTier;
  sinceChangeSec_ = 0.0f;
  overSec_ = 0.0f;
  underSec_ = 0.0f;
}

VolumeCloudSettings VolumeCloudQuality::Apply(const VolumeCloudSettings &base) const {
  VolumeCloudSettings result = base;
  if (!config_.enabled) {
    return result;
  }
  const TierCap &cap = kTierCaps[std::clamp(tier_, 0, kTierCount - 1)];
  result.resolutionDivisor = std::clamp((std::max)(base.resolutionDivisor, cap.minDivisor), kVolumeCloudMinResolutionDivisor,
                                        kVolumeCloudMaxResolutionDivisor);
  result.steps = (std::min)(base.steps, cap.maxSteps);
  result.lightSteps = (std::min)(base.lightSteps, cap.maxLightSteps);
  return result;
}
