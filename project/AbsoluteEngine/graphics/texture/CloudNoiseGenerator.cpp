#include "CloudNoiseGenerator.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <random>
#include <thread>
#include <vector>

// 3D ノイズの仕組み:
//   Perlin ノイズ: 格子点ごとにランダムな向き(勾配)を持たせ、その間をなめらかに結ぶ。なめらかな起伏になる。
//   Worley ノイズ: 空間をマスに区切り、各マスにランダムな点を 1 つ置く。「一番近い点までの距離」を値にする。
//                  距離を反転(1 - 距離)すると、点の近くが高い「丸い塊」の集まりになる(積雲の粒に似る)。
//   タイル可能にする方法: 格子の番号を周期で折り返す(周期 P のとき、番号 P は番号 0 と同じものとして扱う)。
//   fbm: 周波数を 2 倍ずつにして重ねる。周期も 2 倍ずつにするので、タイル可能のまま。

namespace {

constexpr uint32_t kChannelCount = 4;
constexpr float kUnitMax = 16777216.0f; // 24 ビット。ハッシュの上位 24 ビットを 0〜1 にするための割り値

// --- 基本の形 ---
constexpr int32_t kShapePerlinBasePeriod = 4;   // Perlin の最初の周期(1 辺あたりの格子の数)
constexpr int32_t kShapeWorleyBasePeriod = 4;   // R の Worley の最初の周期
constexpr uint32_t kShapeFbmOctaves = 3;        // R で重ねる層の数
constexpr int32_t kShapeWorleyPeriods[3] = {4, 8, 16}; // G, B, A の Worley の周期

// --- 細かい模様 ---
constexpr int32_t kDetailWorleyPeriods[3] = {2, 4, 8}; // R, G, B の Worley の周期

constexpr float kWorleyMaxDistance = 1.0f; // 最近点までの距離をこの値で割って 0〜1 にする(3 次元では 1 近辺が上限)
constexpr float kFbmAmplitudeDecay = 0.5f; // 層が細かくなるたびに、振幅をこの倍率にする
constexpr float kPerlinToUnitScale = 0.5f; // Perlin の -1〜1 を 0〜1 に写す(* 0.5 + 0.5)
constexpr float kByteMax = 255.0f;

// ノイズの種(チャンネルや層ごとにずらして、模様が重なって相関しないようにする)
constexpr uint32_t kSeedPerlin = 11u;
constexpr uint32_t kSeedWorleyFbm = 23u;
constexpr uint32_t kSeedWorleyChannel = 37u;
constexpr uint32_t kSeedStride = 101u; // 層ごとに足す量

// Perlin の勾配(立方体の 12 本の辺の向き)
constexpr std::array<std::array<float, 3>, 12> kGradients = {{
    {{1.0f, 1.0f, 0.0f}},  {{-1.0f, 1.0f, 0.0f}},  {{1.0f, -1.0f, 0.0f}}, {{-1.0f, -1.0f, 0.0f}},
    {{1.0f, 0.0f, 1.0f}},  {{-1.0f, 0.0f, 1.0f}},  {{1.0f, 0.0f, -1.0f}}, {{-1.0f, 0.0f, -1.0f}},
    {{0.0f, 1.0f, 1.0f}},  {{0.0f, -1.0f, 1.0f}},  {{0.0f, 1.0f, -1.0f}}, {{0.0f, -1.0f, -1.0f}},
}};

// 整数のハッシュ(PCG 系)。同じ入力には必ず同じ値を返す = 毎回同じノイズになる
uint32_t Pcg(uint32_t v) {
  const uint32_t state = v * 747796405u + 2891336453u;
  const uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

uint32_t Hash3(int32_t x, int32_t y, int32_t z, uint32_t seed) {
  return Pcg(Pcg(Pcg(static_cast<uint32_t>(x) + seed) + static_cast<uint32_t>(y)) + static_cast<uint32_t>(z));
}

// ハッシュ値の上位 24 ビットを 0〜1 未満の小数にする
float ToUnit(uint32_t h) { return static_cast<float>(h >> 8u) / kUnitMax; }

// 格子番号 i を 0〜period-1 に折り返す(負の数にも対応)。これがタイル可能にする仕掛け
int32_t Wrap(int32_t i, int32_t period) { return ((i % period) + period) % period; }

float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

// 周期 period の Perlin ノイズ。(u, v, w) は 0〜1 の位置。戻り値は 0〜1
float PerlinTile(float u, float v, float w, int32_t period, uint32_t seed) {
  const float x = u * static_cast<float>(period);
  const float y = v * static_cast<float>(period);
  const float z = w * static_cast<float>(period);
  const int32_t ix = static_cast<int32_t>(std::floor(x));
  const int32_t iy = static_cast<int32_t>(std::floor(y));
  const int32_t iz = static_cast<int32_t>(std::floor(z));
  const float fx = x - static_cast<float>(ix);
  const float fy = y - static_cast<float>(iy);
  const float fz = z - static_cast<float>(iz);

  // 立方体の 8 つの角それぞれについて、勾配と「角から点へのベクトル」の内積を求める
  float corner[2][2][2];
  for (int32_t dz = 0; dz < 2; ++dz) {
    for (int32_t dy = 0; dy < 2; ++dy) {
      for (int32_t dx = 0; dx < 2; ++dx) {
        const uint32_t h = Hash3(Wrap(ix + dx, period), Wrap(iy + dy, period), Wrap(iz + dz, period), seed);
        const auto &g = kGradients[h % kGradients.size()];
        corner[dz][dy][dx] = g[0] * (fx - static_cast<float>(dx)) + g[1] * (fy - static_cast<float>(dy)) + g[2] * (fz - static_cast<float>(dz));
      }
    }
  }

  // なめらかな重みで、3 方向に順に混ぜる
  const float ux = Fade(fx);
  const float uy = Fade(fy);
  const float uz = Fade(fz);
  float layer[2];
  for (int32_t dz = 0; dz < 2; ++dz) {
    const float row0 = std::lerp(corner[dz][0][0], corner[dz][0][1], ux);
    const float row1 = std::lerp(corner[dz][1][0], corner[dz][1][1], ux);
    layer[dz] = std::lerp(row0, row1, uy);
  }
  const float n = std::lerp(layer[0], layer[1], uz); // おおよそ -1〜1
  return Saturate(n * kPerlinToUnitScale + kPerlinToUnitScale);
}

// 周期 period の Worley ノイズ(反転)。点の近くほど 1 に近い。戻り値は 0〜1
float WorleyInvertedTile(float u, float v, float w, int32_t period, uint32_t seed) {
  const float x = u * static_cast<float>(period);
  const float y = v * static_cast<float>(period);
  const float z = w * static_cast<float>(period);
  const int32_t ix = static_cast<int32_t>(std::floor(x));
  const int32_t iy = static_cast<int32_t>(std::floor(y));
  const int32_t iz = static_cast<int32_t>(std::floor(z));
  const float fx = x - static_cast<float>(ix);
  const float fy = y - static_cast<float>(iy);
  const float fz = z - static_cast<float>(iz);

  // 自分のマスとまわり 26 マス(3x3x3)の点を調べ、一番近いものまでの距離(の 2 乗)を探す
  float minDistSq = 1.0e9f;
  for (int32_t dz = -1; dz <= 1; ++dz) {
    for (int32_t dy = -1; dy <= 1; ++dy) {
      for (int32_t dx = -1; dx <= 1; ++dx) {
        const uint32_t h = Hash3(Wrap(ix + dx, period), Wrap(iy + dy, period), Wrap(iz + dz, period), seed);
        // マス内の点の位置。3 つの値を、ハッシュを重ねて作る
        const float px = static_cast<float>(dx) + ToUnit(h) - fx;
        const float py = static_cast<float>(dy) + ToUnit(Pcg(h + 1u)) - fy;
        const float pz = static_cast<float>(dz) + ToUnit(Pcg(h + 2u)) - fz;
        minDistSq = (std::min)(minDistSq, px * px + py * py + pz * pz);
      }
    }
  }
  return 1.0f - Saturate(std::sqrt(minDistSq) / kWorleyMaxDistance);
}

// fbm 用に、層の重みの合計で割って 0〜1 に収める
float PerlinFbm(float u, float v, float w, int32_t basePeriod, uint32_t octaves) {
  float sum = 0.0f;
  float weightSum = 0.0f;
  float amplitude = 1.0f;
  int32_t period = basePeriod;
  for (uint32_t i = 0; i < octaves; ++i) {
    sum += amplitude * PerlinTile(u, v, w, period, kSeedPerlin + i * kSeedStride);
    weightSum += amplitude;
    amplitude *= kFbmAmplitudeDecay;
    period *= 2;
  }
  return sum / weightSum;
}

float WorleyFbm(float u, float v, float w, int32_t basePeriod, uint32_t octaves) {
  float sum = 0.0f;
  float weightSum = 0.0f;
  float amplitude = 1.0f;
  int32_t period = basePeriod;
  for (uint32_t i = 0; i < octaves; ++i) {
    sum += amplitude * WorleyInvertedTile(u, v, w, period, kSeedWorleyFbm + i * kSeedStride);
    weightSum += amplitude;
    amplitude *= kFbmAmplitudeDecay;
    period *= 2;
  }
  return sum / weightSum;
}

// 値を [newLow, newHigh] へ写す(入力は 0〜1)。Perlin を Worley の値から 1 の間へ押し上げるのに使う
float Remap01(float v, float newLow, float newHigh) { return newLow + v * (newHigh - newLow); }

uint8_t ToByte(float v) { return static_cast<uint8_t>(Saturate(v) * kByteMax + 0.5f); }

// z 方向のスライスを複数のスレッドで分担して処理する(CPU の全コアを使って起動時間を縮める)
template <class Fn> void ParallelForSlices(uint32_t sliceCount, Fn &&fn) {
  const uint32_t hardware = (std::max)(1u, std::thread::hardware_concurrency());
  const uint32_t threadCount = (std::min)(hardware, sliceCount);
  std::atomic<uint32_t> next{0};
  std::vector<std::thread> threads;
  threads.reserve(threadCount);
  for (uint32_t t = 0; t < threadCount; ++t) {
    threads.emplace_back([&]() {
      for (;;) {
        const uint32_t z = next.fetch_add(1);
        if (z >= sliceCount) {
          break;
        }
        fn(z);
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
}

// size^3 の画像を作り、各画素の 4 チャンネルを sampler(u, v, w) で埋める。sampler は 0〜1 の位置を受け取り、4 値を返す
template <class Sampler> DirectX::ScratchImage GenerateVolume(uint32_t size, Sampler &&sampler) {
  DirectX::ScratchImage image;
  if (size == 0 || FAILED(image.Initialize3D(DXGI_FORMAT_R8G8B8A8_UNORM, size, size, size, 1))) {
    return DirectX::ScratchImage{};
  }

  ParallelForSlices(size, [&](uint32_t z) {
    const DirectX::Image *slice = image.GetImage(0, 0, z);
    const float w = (static_cast<float>(z) + 0.5f) / static_cast<float>(size); // 画素の中心の位置
    for (uint32_t y = 0; y < size; ++y) {
      uint8_t *row = slice->pixels + static_cast<size_t>(y) * slice->rowPitch;
      const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
      for (uint32_t x = 0; x < size; ++x) {
        const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
        const std::array<float, kChannelCount> value = sampler(u, v, w);
        for (uint32_t c = 0; c < kChannelCount; ++c) {
          row[x * kChannelCount + c] = ToByte(value[c]);
        }
      }
    }
  });
  return image;
}

// --- 青色雑音(void-and-cluster 法。Ulichney 1993) ---
// 仕組み: 点が「詰まった所(cluster)」から取り除き、「空いた所(void)」へ足す、を繰り返して、点を均等にばらけさせる。
//         点を取り除く順 / 足す順を、そのまま画素の順位(0 〜 画素数-1)にする。順位が近い画素は、離れて配置される。
// 「詰まり具合」は、ガウス関数で近くの点の影響を足した値(エネルギー)で測る。端はつながっているものとして扱う(タイル可能)。
constexpr float kBlueNoiseSigma = 1.9f;            // ガウスの広がり(画素)
constexpr int32_t kBlueNoiseKernelRadius = 6;      // ガウスを計算する範囲の半径(約 3 sigma)
constexpr float kBlueNoiseInitialDensity = 0.1f;   // 最初に置く点の割合
constexpr uint32_t kBlueNoiseSeed = 20261018u;     // 乱数の種(毎回同じ模様になる)
constexpr int32_t kBlueNoiseMinSize = 16;          // これより小さいと、ガウスの範囲が 1 周して重なる

// size x size の画素の順位(0 〜 size*size-1)を、画素番号(y * size + x)の順で返す
std::vector<int32_t> BuildBlueNoiseRanks(int32_t size) {
  const int32_t count = size * size;

  // ガウスの核(中心からの距離で決まる重み)
  const int32_t kernelWidth = kBlueNoiseKernelRadius * 2 + 1;
  std::vector<float> kernel(static_cast<size_t>(kernelWidth) * kernelWidth);
  for (int32_t dy = -kBlueNoiseKernelRadius; dy <= kBlueNoiseKernelRadius; ++dy) {
    for (int32_t dx = -kBlueNoiseKernelRadius; dx <= kBlueNoiseKernelRadius; ++dx) {
      const float distanceSq = static_cast<float>(dx * dx + dy * dy);
      kernel[static_cast<size_t>(dy + kBlueNoiseKernelRadius) * kernelWidth + (dx + kBlueNoiseKernelRadius)] =
          std::exp(-distanceSq / (2.0f * kBlueNoiseSigma * kBlueNoiseSigma));
    }
  }

  std::vector<uint8_t> pattern(count, 0); // 1 = 点がある
  std::vector<float> energy(count, 0.0f);  // 各画素の、近くの点の影響の合計(大きいほど詰まっている)

  // 点を足す(sign = +1)/ 取り除く(sign = -1)ときに、まわりのエネルギーを更新する
  auto splat = [&](int32_t index, float sign) {
    const int32_t x = index % size;
    const int32_t y = index / size;
    for (int32_t dy = -kBlueNoiseKernelRadius; dy <= kBlueNoiseKernelRadius; ++dy) {
      const int32_t yy = ((y + dy) % size + size) % size;
      for (int32_t dx = -kBlueNoiseKernelRadius; dx <= kBlueNoiseKernelRadius; ++dx) {
        const int32_t xx = ((x + dx) % size + size) % size;
        energy[static_cast<size_t>(yy) * size + xx] +=
            sign * kernel[static_cast<size_t>(dy + kBlueNoiseKernelRadius) * kernelWidth + (dx + kBlueNoiseKernelRadius)];
      }
    }
  };
  // 点がある画素のうち、一番詰まっている(エネルギーが最大の)画素
  auto findTightestCluster = [&]() {
    int32_t best = -1;
    float bestEnergy = -(std::numeric_limits<float>::max)();
    for (int32_t i = 0; i < count; ++i) {
      if (pattern[i] != 0 && energy[i] > bestEnergy) {
        bestEnergy = energy[i];
        best = i;
      }
    }
    return best;
  };
  // 点がない画素のうち、一番空いている(エネルギーが最小の)画素
  auto findLargestVoid = [&]() {
    int32_t best = -1;
    float bestEnergy = (std::numeric_limits<float>::max)();
    for (int32_t i = 0; i < count; ++i) {
      if (pattern[i] == 0 && energy[i] < bestEnergy) {
        bestEnergy = energy[i];
        best = i;
      }
    }
    return best;
  };

  // 初期パターン: 乱数で、一定の割合の点を置く
  std::vector<int32_t> order(count);
  for (int32_t i = 0; i < count; ++i) {
    order[i] = i;
  }
  std::mt19937 random(kBlueNoiseSeed);
  std::shuffle(order.begin(), order.end(), random);
  const int32_t initialOnes = (std::max)(1, static_cast<int32_t>(static_cast<float>(count) * kBlueNoiseInitialDensity));
  for (int32_t k = 0; k < initialOnes; ++k) {
    pattern[order[k]] = 1;
    splat(order[k], 1.0f);
  }

  // 緩和: 一番詰まった点を、一番空いた所へ動かす。動かす先が元の位置と同じになったら、落ち着いたとみなして止める
  for (int32_t iteration = 0; iteration < count; ++iteration) {
    const int32_t cluster = findTightestCluster();
    pattern[cluster] = 0;
    splat(cluster, -1.0f);
    const int32_t voidIndex = findLargestVoid();
    pattern[voidIndex] = 1;
    splat(voidIndex, 1.0f);
    if (voidIndex == cluster) {
      break;
    }
  }
  const std::vector<uint8_t> relaxedPattern = pattern;
  const std::vector<float> relaxedEnergy = energy;

  std::vector<int32_t> ranks(count, 0);

  // 段階 1: 初期の点を、一番詰まった順に取り除き、順位を大きい方(initialOnes - 1)から小さい方へ割り当てる
  for (int32_t rank = initialOnes - 1; rank >= 0; --rank) {
    const int32_t cluster = findTightestCluster();
    ranks[cluster] = rank;
    pattern[cluster] = 0;
    splat(cluster, -1.0f);
  }

  // 段階 2: 初期パターンへ戻し、一番空いた所へ順に点を足して、残りの順位(initialOnes 〜 最後)を割り当てる
  pattern = relaxedPattern;
  energy = relaxedEnergy;
  for (int32_t rank = initialOnes; rank < count; ++rank) {
    const int32_t voidIndex = findLargestVoid();
    ranks[voidIndex] = rank;
    pattern[voidIndex] = 1;
    splat(voidIndex, 1.0f);
  }
  return ranks;
}

} // namespace

namespace CloudNoiseGenerator {

DirectX::ScratchImage GenerateBlueNoise(uint32_t size) {
  if (static_cast<int32_t>(size) < kBlueNoiseMinSize) {
    return DirectX::ScratchImage{};
  }
  DirectX::ScratchImage image;
  if (FAILED(image.Initialize2D(DXGI_FORMAT_R8_UNORM, size, size, 1, 1))) {
    return DirectX::ScratchImage{};
  }
  const int32_t count = static_cast<int32_t>(size * size);
  const std::vector<int32_t> ranks = BuildBlueNoiseRanks(static_cast<int32_t>(size));
  const DirectX::Image *pixels = image.GetImage(0, 0, 0);
  for (uint32_t y = 0; y < size; ++y) {
    uint8_t *row = pixels->pixels + static_cast<size_t>(y) * pixels->rowPitch;
    for (uint32_t x = 0; x < size; ++x) {
      // 順位(0 〜 count-1)を、0〜1 に一様に分布する値へ
      row[x] = ToByte(static_cast<float>(ranks[static_cast<size_t>(y) * size + x]) / static_cast<float>(count - 1));
    }
  }
  return image;
}

DirectX::ScratchImage GenerateShape(uint32_t size) {
  return GenerateVolume(size, [](float u, float v, float w) {
    // R: Perlin の起伏を、Worley の塊の値から 1 の間へ押し上げる(塊の形に沿った、起伏のある雲の形になる)
    const float perlin = PerlinFbm(u, v, w, kShapePerlinBasePeriod, kShapeFbmOctaves);
    const float worley = WorleyFbm(u, v, w, kShapeWorleyBasePeriod, kShapeFbmOctaves);
    std::array<float, kChannelCount> value{};
    value[0] = Remap01(perlin, worley, 1.0f);
    // G, B, A: 細かさの違う Worley
    for (uint32_t c = 0; c < 3; ++c) {
      value[c + 1] = WorleyInvertedTile(u, v, w, kShapeWorleyPeriods[c], kSeedWorleyChannel + c * kSeedStride);
    }
    return value;
  });
}

DirectX::ScratchImage GenerateDetail(uint32_t size) {
  return GenerateVolume(size, [](float u, float v, float w) {
    std::array<float, kChannelCount> value{};
    for (uint32_t c = 0; c < 3; ++c) {
      value[c] = WorleyInvertedTile(u, v, w, kDetailWorleyPeriods[c], kSeedWorleyChannel + (c + 3u) * kSeedStride);
    }
    value[3] = 1.0f;
    return value;
  });
}

} // namespace CloudNoiseGenerator
