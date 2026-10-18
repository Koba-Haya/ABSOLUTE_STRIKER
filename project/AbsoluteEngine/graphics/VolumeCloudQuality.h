#pragma once
#include "VolumeCloudSettings.h"
#include <cstdint>

// ボリューム雲の品質の自動調整の設定
struct VolumeCloudQualityConfig {
  bool enabled = true;
  // GPU の 1 フレームの時間(ミリ秒)の移動平均が downMs を超えた状態が downHoldSec 続いたら、品質を 1 段下げる。
  // 60fps の枠は 16.6ms なので、その手前(13ms)で下げ始める
  float downMs = 13.0f;
  // 移動平均が upMs を下回った状態が upHoldSec 続き、かつ最後に段を変えてから upWaitSec が過ぎたら、1 段上げる
  float upMs = 8.0f;
  float downHoldSec = 1.0f;
  float upHoldSec = 5.0f;
  float baseUpWaitSec = 10.0f; // 段を変えてから、上げるまでの最短の待ち時間
  float warmupSec = 2.0f;      // 雲の描画を始めてから(または再開してから)、判断を見送る時間。GPU のクロックが安定するまで待つ
};

// ボリューム雲の品質を、実測した GPU 時間に合わせて自動で上げ下げするクラス。
// ノートPCは、電源の状態や熱で GPU の速さが大きく変わる。重くなったら雲の解像度やステップ数を下げ、余裕が戻れば上げる。
// 段は 0 が最高(呼び出し側の設定そのまま)で、数字が大きいほど軽い。
// 呼び出し側の設定(VolumeCloudSettings)は「基準」として書き換えず、描く直前の値だけを Apply で上書きする。
// このクラスは描画(Renderer)や計測器(GpuTimer)を知らない。測った時間を受け取って、段と上書きした設定を返すだけ。
class VolumeCloudQuality {
public:
  static constexpr int32_t kTierCount = 4;

  // 毎フレーム 1 回呼ぶ。frameGpuMs: 直近の GPU の 1 フレームの時間(ミリ秒)。
  // deltaSec: 前回の呼び出しからの経過(秒)。最初の呼び出しは 0 でよい。
  // 0.5 秒以上空いたら(雲を描かない別のシーンなど)、再開とみなして判断を見送る時間からやり直す
  void Update(float frameGpuMs, float deltaSec);

  // 基準の設定 base を、今の段に合わせて軽くした設定を返す(自動調整がオフ、または段 0 のときは base のまま。base より重くはしない)
  VolumeCloudSettings Apply(const VolumeCloudSettings &base) const;

  int32_t GetTier() const { return tier_; }
  float GetSmoothedMs() const { return smoothedMs_; }
  VolumeCloudQualityConfig &GetConfig() { return config_; }
  const VolumeCloudQualityConfig &GetConfig() const { return config_; }

private:
  void ChangeTier_(int32_t newTier);

  VolumeCloudQualityConfig config_{};
  int32_t tier_ = 0;
  float smoothedMs_ = 0.0f;
  float overSec_ = 0.0f;        // 平均が downMs を超えている時間
  float underSec_ = 0.0f;       // 平均が upMs を下回っている時間
  float sinceChangeSec_ = 0.0f; // 最後に段を変えてからの時間
  float warmupLeftSec_ = 0.0f;
  float upWaitSec_ = 0.0f;      // 今の、上げるまでの待ち時間(上げてすぐ下がったら倍にする)
  bool lastChangeWasUp_ = false;
  bool started_ = false;        // 最初の Update が済んだか
};
