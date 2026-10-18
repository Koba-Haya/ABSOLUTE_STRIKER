#include "GpuTimer.h"
#include "Logger.h"
#include "externals/DirectXTex/d3dx12.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <string>

namespace {
constexpr uint32_t kMaxSections = 32;                    // 1 フレームに測れる区間の数
constexpr uint32_t kQueriesPerSection = 2;               // 区間 1 つにつき、開始と終了の 2 つ
constexpr uint32_t kMaxQueries = kMaxSections * kQueriesPerSection;
constexpr double kReportIntervalSec = 2.0;               // 何秒ごとに集計してログへ出すか
constexpr double kMsPerSec = 1000.0;
} // namespace

bool GpuTimer::Initialize(ID3D12Device *device, ID3D12CommandQueue *queue) {
  available_ = false;
  if (!device || !queue) {
    return false;
  }

  // GPU の時計の周波数(1 秒あたりの目盛り数)。これで目盛りの差をミリ秒へ換算する
  UINT64 gpuFrequency = 0;
  if (FAILED(queue->GetTimestampFrequency(&gpuFrequency)) || gpuFrequency == 0) {
    Logger::GetInstance().Log("[GpuTimer] タイムスタンプの周波数を取得できないため、計測を無効にします\n");
    return false;
  }
  ticksToMs_ = kMsPerSec / static_cast<double>(gpuFrequency);

  // タイムスタンプを書き込む先(クエリヒープ)
  D3D12_QUERY_HEAP_DESC heapDesc{};
  heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
  heapDesc.Count = kMaxQueries;
  if (FAILED(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_)))) {
    Logger::GetInstance().Log("[GpuTimer] クエリヒープを作れないため、計測を無効にします\n");
    return false;
  }

  // CPU から読める読み戻しバッファ。クエリヒープは直接読めないので、Resolve でここへ書き出す
  const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_READBACK);
  const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * kMaxQueries);
  if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer_)))) {
    Logger::GetInstance().Log("[GpuTimer] 読み戻しバッファを作れないため、計測を無効にします\n");
    queryHeap_.Reset();
    return false;
  }

  // 報告間隔と fps の計算に使う CPU 側の時計
  LARGE_INTEGER frequency{};
  LARGE_INTEGER now{};
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&now);
  counterFrequency_ = frequency.QuadPart;
  intervalStartTicks_ = now.QuadPart;

  slots_.reserve(kMaxSections);
  available_ = true;
  return true;
}

int32_t GpuTimer::Begin(ID3D12GraphicsCommandList *cmdList, const char *label) {
  if (!available_ || !cmdList || slots_.size() >= kMaxSections) {
    return -1;
  }
  const int32_t slot = static_cast<int32_t>(slots_.size());
  slots_.push_back({label, false});
  cmdList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(slot) * kQueriesPerSection);
  return slot;
}

void GpuTimer::End(ID3D12GraphicsCommandList *cmdList, int32_t slot) {
  if (!available_ || !cmdList || slot < 0 || static_cast<size_t>(slot) >= slots_.size()) {
    return;
  }
  cmdList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(slot) * kQueriesPerSection + 1);
  slots_[slot].ended = true;
}

void GpuTimer::Resolve(ID3D12GraphicsCommandList *cmdList) {
  if (!available_ || !cmdList || slots_.empty()) {
    return;
  }
  const UINT queryCount = static_cast<UINT>(slots_.size()) * kQueriesPerSection;
  cmdList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, queryCount, readbackBuffer_.Get(), 0);
}

void GpuTimer::Collect() {
  if (!available_) {
    return;
  }

  // 1 フレーム分の結果を読む(GPU の完了待ちのあとなので、読み戻しバッファは書き終わっている)
  if (!slots_.empty()) {
    const SIZE_T readBytes = sizeof(uint64_t) * kQueriesPerSection * slots_.size();
    const D3D12_RANGE readRange{0, readBytes};
    void *mapped = nullptr;
    if (SUCCEEDED(readbackBuffer_->Map(0, &readRange, &mapped))) {
      const uint64_t *timestamps = static_cast<const uint64_t *>(mapped);
      // 同じ名前の区間が 1 フレームに複数回あれば、合計して 1 つの値にする
      std::vector<Accumulator> frame;
      for (size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].ended) {
          continue;
        }
        const uint64_t begin = timestamps[i * kQueriesPerSection];
        const uint64_t end = timestamps[i * kQueriesPerSection + 1];
        if (end < begin) {
          continue; // 時計が巻き戻る異常値は捨てる
        }
        const double ms = static_cast<double>(end - begin) * ticksToMs_;

        Accumulator *target = nullptr;
        for (auto &entry : frame) {
          if (entry.label == slots_[i].label) {
            target = &entry;
            break;
          }
        }
        if (!target) {
          frame.push_back({slots_[i].label, 0.0, 0.0f});
          target = &frame.back();
        }
        target->sumMs += ms;
      }
      const D3D12_RANGE writtenRange{0, 0}; // 読むだけで書いていないことを伝える
      readbackBuffer_->Unmap(0, &writtenRange);

      // 直近 1 フレームの値として残す(GetLastFrameMs 用)
      lastFrameSections_.clear();
      for (const auto &entry : frame) {
        lastFrameSections_.push_back({entry.label, static_cast<float>(entry.sumMs)});
      }

      // フレームの値を、報告間隔の累積へ足す
      for (const auto &entry : frame) {
        Accumulator *total = nullptr;
        for (auto &acc : accumulators_) {
          if (acc.label == entry.label) {
            total = &acc;
            break;
          }
        }
        if (!total) {
          accumulators_.push_back({entry.label, 0.0, 0.0f});
          total = &accumulators_.back();
        }
        total->sumMs += entry.sumMs;
        total->maxMs = (std::max)(total->maxMs, static_cast<float>(entry.sumMs));
      }
    }
  }
  slots_.clear(); // 次のフレームの枠を空ける
  ++framesInInterval_;

  // 報告間隔が過ぎたら、平均を出してログへ書く
  LARGE_INTEGER now{};
  QueryPerformanceCounter(&now);
  const double elapsedSec = static_cast<double>(now.QuadPart - intervalStartTicks_) / static_cast<double>(counterFrequency_);
  if (elapsedSec >= kReportIntervalSec) {
    Report_(elapsedSec);
    intervalStartTicks_ = now.QuadPart;
  }
}

float GpuTimer::GetLastFrameMs(const char *label) const {
  for (const auto &section : lastFrameSections_) {
    if (section.label == label) {
      return section.ms;
    }
  }
  return 0.0f;
}

void GpuTimer::Report_(double elapsedSec) {
  reportedFps_ = static_cast<float>(framesInInterval_ / elapsedSec);

  reportedStats_.clear();
  char fpsText[32];
  snprintf(fpsText, sizeof(fpsText), "[GpuTimer] fps=%.1f", reportedFps_);
  std::string line = fpsText;
  for (const auto &acc : accumulators_) {
    SectionStats stats;
    stats.label = acc.label;
    stats.averageMs = static_cast<float>(acc.sumMs / framesInInterval_);
    stats.maxMs = acc.maxMs;
    reportedStats_.push_back(stats);

    char text[128];
    snprintf(text, sizeof(text), " | %s avg=%.2fms max=%.2fms", stats.label.c_str(), stats.averageMs, stats.maxMs);
    line += text;
  }
  if (logEnabled_) {
    Logger::GetInstance().Log("%s\n", line.c_str());
  }

  accumulators_.clear();
  framesInInterval_ = 0;
}

GpuTimer::Scope::Scope(GpuTimer &timer, ID3D12GraphicsCommandList *cmdList, const char *label)
    : timer_(timer), cmdList_(cmdList), slot_(timer.Begin(cmdList, label)) {}

GpuTimer::Scope::~Scope() { timer_.End(cmdList_, slot_); }
