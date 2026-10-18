#include "CpuTimer.h"

#include <Windows.h>

#include <map>
#include <string>

#include "Logger.h"

namespace {
// 1 フレームがこの時間を超えたら「停止(スパイク)」とみなして、その場でログに出す(ミリ秒)。
// 60fps の 1 フレームは約 16.7ms なので、約 2 フレーム分の 33ms を目安にしている
constexpr double kSpikeThresholdMs = 33.0;
// 平均と最大をまとめてログに出す間隔(秒)。GpuTimer の報告間隔と同じ 1 秒
constexpr double kReportIntervalSec = 1.0;

// QueryPerformanceCounter の現在値を返す
int64_t NowTicks() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

// QueryPerformanceCounter の周波数(1 秒あたりの目盛り数)。起動中は変わらないので 1 回だけ取得する
int64_t Frequency() {
  static const int64_t freq = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
  }();
  return freq;
}

// 目盛りの差をミリ秒に換算する
double TicksToMs(int64_t ticks) { return static_cast<double>(ticks) * 1000.0 / static_cast<double>(Frequency()); }

// フレーム測定の状態(メインスレッドからだけ触る)
int64_t g_frameStartTicks = 0;
int64_t g_intervalStartTicks = 0;
double g_sumMs = 0.0;
double g_maxMs = 0.0;
uint32_t g_frames = 0;

// AccumScope の集計(ラベルごとの回数と合計ミリ秒)
struct AccumEntry {
  uint32_t count = 0;
  double totalMs = 0.0;
};
std::map<std::string, AccumEntry> g_accum;
} // namespace

CpuTimer::Scope::Scope(const char *label) : label_(label), startTicks_(NowTicks()) {}

CpuTimer::Scope::~Scope() {
  const double ms = TicksToMs(NowTicks() - startTicks_);
  Logger::GetInstance().Log("[CpuTimer] %s = %.2f ms\n", label_, ms);
}

CpuTimer::AccumScope::AccumScope(const char *label) : label_(label), startTicks_(NowTicks()) {}

CpuTimer::AccumScope::~AccumScope() {
  AccumEntry &e = g_accum[label_];
  ++e.count;
  e.totalMs += TicksToMs(NowTicks() - startTicks_);
}

void CpuTimer::FlushAccumulated() {
  for (const auto &kv : g_accum) {
    Logger::GetInstance().Log("[CpuTimer] accum %s : count=%u total=%.2f ms\n", kv.first.c_str(), kv.second.count, kv.second.totalMs);
  }
  g_accum.clear();
}

void CpuTimer::BeginFrame() {
  g_frameStartTicks = NowTicks();
  // 最初のフレームは報告間隔の起点も同時に決める
  if (g_intervalStartTicks == 0) {
    g_intervalStartTicks = g_frameStartTicks;
  }
}

void CpuTimer::EndFrame() {
  const int64_t now = NowTicks();
  const double frameMs = TicksToMs(now - g_frameStartTicks);

  // 停止(スパイク)はその場で出す。1 秒ごとのまとめだけだと、どのタイミングか分からなくなるため
  if (frameMs > kSpikeThresholdMs) {
    Logger::GetInstance().Log("[CpuTimer] frame spike = %.2f ms\n", frameMs);
  }

  g_sumMs += frameMs;
  if (frameMs > g_maxMs) {
    g_maxMs = frameMs;
  }
  ++g_frames;

  // 報告間隔ごとに、1 フレームあたりの平均と最大をログへ出して集計をリセットする
  const double elapsedSec = static_cast<double>(now - g_intervalStartTicks) / static_cast<double>(Frequency());
  if (elapsedSec >= kReportIntervalSec) {
    Logger::GetInstance().Log("[CpuTimer] frame avg=%.2fms max=%.2fms\n", g_sumMs / g_frames, g_maxMs);
    g_sumMs = 0.0;
    g_maxMs = 0.0;
    g_frames = 0;
    g_intervalStartTicks = now;
  }
}
