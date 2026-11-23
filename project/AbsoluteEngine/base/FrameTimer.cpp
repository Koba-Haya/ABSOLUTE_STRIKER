#include "FrameTimer.h"

#include "CpuTimer.h"
#include "Logger.h"

namespace {
// 1 フレームがこの時間を超えたら「停止(スパイク)」とみなして、その場でログに出す(ミリ秒)。
// 60fps の 1 フレームは約 16.7ms なので、約 2 フレーム分の 33ms を目安にしている
constexpr double kSpikeThresholdMs = 33.0;
// 平均と最大をまとめてログに出す間隔(ミリ秒)。GpuTimer の報告間隔と同じ 1 秒
constexpr double kReportIntervalMs = 1000.0;
} // namespace

void FrameTimer::BeginFrame() {
  frameStartTicks_ = CpuTimer::NowTicks();
  // 最初のフレームは報告間隔の起点も同時に決める
  if (intervalStartTicks_ == 0) {
    intervalStartTicks_ = frameStartTicks_;
  }
}

void FrameTimer::EndFrame() {
  const int64_t now = CpuTimer::NowTicks();
  const double frameMs = CpuTimer::TicksToMs(now - frameStartTicks_);

  // 停止(スパイク)はその場で出す。1 秒ごとのまとめだけだと、どのタイミングか分からなくなるため
  if (frameMs > kSpikeThresholdMs) {
    Logger::GetInstance().Log("[CpuTimer] frame spike = %.2f ms\n", frameMs);
  }

  sumMs_ += frameMs;
  if (frameMs > maxMs_) {
    maxMs_ = frameMs;
  }
  ++frames_;

  // 報告間隔ごとに、1 フレームあたりの平均と最大をログへ出して集計をリセットする
  if (CpuTimer::TicksToMs(now - intervalStartTicks_) >= kReportIntervalMs) {
    Logger::GetInstance().Log("[CpuTimer] frame avg=%.2fms max=%.2fms\n", sumMs_ / frames_, maxMs_);
    sumMs_ = 0.0;
    maxMs_ = 0.0;
    frames_ = 0;
    intervalStartTicks_ = now;
  }
}
