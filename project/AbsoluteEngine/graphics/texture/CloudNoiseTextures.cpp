#include "CloudNoiseTextures.h"
#include "CloudNoiseGenerator.h"
#include "Logger.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace {

constexpr uint32_t kShapeSize = 64;  // 基本の形の 1 辺の画素数(64^3 * 4 バイト = 約 1MB)
constexpr uint32_t kDetailSize = 32; // 細かい模様の 1 辺の画素数
constexpr uint32_t kBlueNoiseSize = 64; // 青色雑音の 1 辺の画素数(タイルして繰り返す)
constexpr uint32_t kChannelCount = 4;

// 確認用: true にすると、生成したノイズの中央のスライスを PNG にして実行フォルダへ書く(見た目の確認専用)
constexpr bool kDumpDebugSlices = false;
constexpr uint32_t kDebugPanelPixels = 256; // 1 チャンネルの表示サイズ(最近傍で拡大)
constexpr uint8_t kOpaqueAlpha = 255;

// 各チャンネルの最小・最大・平均をログへ出す(値が偏っていないか、全部同じ値になっていないかを数字で確認する)
void LogChannelStats(const char *name, const DirectX::ScratchImage &image) {
  const DirectX::TexMetadata &meta = image.GetMetadata();
  std::array<uint8_t, kChannelCount> minValue;
  std::array<uint8_t, kChannelCount> maxValue;
  std::array<double, kChannelCount> sum{};
  minValue.fill(UINT8_MAX);
  maxValue.fill(0);

  const size_t voxelCount = meta.width * meta.height * meta.depth;
  for (size_t z = 0; z < meta.depth; ++z) {
    const DirectX::Image *slice = image.GetImage(0, 0, z);
    for (size_t y = 0; y < meta.height; ++y) {
      const uint8_t *row = slice->pixels + y * slice->rowPitch;
      for (size_t x = 0; x < meta.width; ++x) {
        for (uint32_t c = 0; c < kChannelCount; ++c) {
          const uint8_t value = row[x * kChannelCount + c];
          minValue[c] = (std::min)(minValue[c], value);
          maxValue[c] = (std::max)(maxValue[c], value);
          sum[c] += value;
        }
      }
    }
  }
  Logger::GetInstance().Log("[CloudNoise] %s %zux%zux%zu  R(min=%u max=%u avg=%.0f) G(min=%u max=%u avg=%.0f) B(min=%u max=%u avg=%.0f) A(min=%u max=%u avg=%.0f)\n",
                            name, meta.width, meta.height, meta.depth,
                            minValue[0], maxValue[0], sum[0] / voxelCount,
                            minValue[1], maxValue[1], sum[1] / voxelCount,
                            minValue[2], maxValue[2], sum[2] / voxelCount,
                            minValue[3], maxValue[3], sum[3] / voxelCount);
}

// 中央のスライスの 4 チャンネルを、白黒の 4 枚並びの PNG にして書き出す(確認専用)
void DumpMiddleSlice(const char *fileName, const DirectX::ScratchImage &volume) {
  const DirectX::TexMetadata &meta = volume.GetMetadata();
  const DirectX::Image *slice = volume.GetImage(0, 0, meta.depth / 2);

  DirectX::ScratchImage panel;
  const size_t outWidth = static_cast<size_t>(kDebugPanelPixels) * kChannelCount;
  if (FAILED(panel.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, outWidth, kDebugPanelPixels, 1, 1))) {
    return;
  }
  const DirectX::Image *out = panel.GetImage(0, 0, 0);
  for (uint32_t c = 0; c < kChannelCount; ++c) {
    for (uint32_t py = 0; py < kDebugPanelPixels; ++py) {
      const size_t sy = static_cast<size_t>(py) * meta.height / kDebugPanelPixels;
      const uint8_t *srcRow = slice->pixels + sy * slice->rowPitch;
      uint8_t *dstRow = out->pixels + static_cast<size_t>(py) * out->rowPitch;
      for (uint32_t px = 0; px < kDebugPanelPixels; ++px) {
        const size_t sx = static_cast<size_t>(px) * meta.width / kDebugPanelPixels;
        const uint8_t value = srcRow[sx * kChannelCount + c];
        uint8_t *dst = dstRow + (static_cast<size_t>(c) * kDebugPanelPixels + px) * kChannelCount;
        dst[0] = value;
        dst[1] = value;
        dst[2] = value;
        dst[3] = kOpaqueAlpha;
      }
    }
  }

  const std::wstring path = std::wstring(fileName, fileName + std::strlen(fileName));
  DirectX::SaveToWICFile(*out, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str());
}

} // namespace

bool CloudNoiseTextures::Initialize(DirectXCommon *dx) {
  initialized_ = false;

  // 1. CPU でノイズを生成する(時間を測ってログへ。起動が重くなっていないかの確認用)
  const auto start = std::chrono::steady_clock::now();
  DirectX::ScratchImage shape = CloudNoiseGenerator::GenerateShape(kShapeSize);
  DirectX::ScratchImage detail = CloudNoiseGenerator::GenerateDetail(kDetailSize);
  DirectX::ScratchImage blueNoise = CloudNoiseGenerator::GenerateBlueNoise(kBlueNoiseSize);
  const auto generated = std::chrono::steady_clock::now();
  if (shape.GetImageCount() == 0 || detail.GetImageCount() == 0 || blueNoise.GetImageCount() == 0) {
    Logger::GetInstance().Log("[CloudNoise] ノイズの生成に失敗しました\n");
    return false;
  }

  // 2. GPU のテクスチャを作り、転送の命令を積む
  if (!shape_.CreateFromMetadata(dx, shape, shape.GetMetadata()) ||
      !detail_.CreateFromMetadata(dx, detail, detail.GetMetadata()) ||
      !blueNoise_.CreateFromMetadata(dx, blueNoise, blueNoise.GetMetadata())) {
    Logger::GetInstance().Log("[CloudNoise] GPU テクスチャの作成に失敗しました\n");
    return false;
  }
  const auto uploaded = std::chrono::steady_clock::now();

  using Ms = std::chrono::duration<double, std::milli>;
  Logger::GetInstance().Log("[CloudNoise] 生成 %.1fms / GPU への転送命令 %.1fms\n",
                            Ms(generated - start).count(), Ms(uploaded - generated).count());
  LogChannelStats("shape", shape);
  LogChannelStats("detail", detail);

  if (kDumpDebugSlices) {
    DumpMiddleSlice("cloud_noise_shape_slice.png", shape);
    DumpMiddleSlice("cloud_noise_detail_slice.png", detail);
  }

  initialized_ = true;
  return true;
}
