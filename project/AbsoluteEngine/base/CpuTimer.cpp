#include "CpuTimer.h"

#include <Windows.h>

#include "Logger.h"

namespace {
// QueryPerformanceCounter の周波数(1 秒あたりの目盛り数)。起動中は変わらないので 1 回だけ取得する
int64_t Frequency() {
  static const int64_t freq = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
  }();
  return freq;
}
} // namespace

int64_t CpuTimer::NowTicks() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

double CpuTimer::TicksToMs(int64_t ticks) { return static_cast<double>(ticks) * 1000.0 / static_cast<double>(Frequency()); }

CpuTimer::Scope::Scope(const char *label) : label_(label), startTicks_(NowTicks()) {}

CpuTimer::Scope::~Scope() {
  const double ms = TicksToMs(NowTicks() - startTicks_);
  Logger::GetInstance().Log("[CpuTimer] %s = %.2f ms\n", label_, ms);
}
