#ifndef CLOUD_SEA_HLSLI
#define CLOUD_SEA_HLSLI

#include "Noise.hlsli"

// 雲海(ゲーム中に、自機の下に広がる雲の海)を返す関数群。
// 「高さ場レイマーチ」: 雲の上面を y = 高さ(x, z)、下面を y = -gThickness * 濃さ(x, z) とみなし、
// 視線がその間に入る位置を数ステップで探す。体積を積算しないので軽い。
// 厚みは、見つけた点から下面までの距離を濃さ(gDensity)で透過率へ換算して表す。
// 立体感は、面の向き(法線)・柱の中の高さ・雲同士の影(太陽方向の高さ場を 4 回調べる)でつける。
//
// 座標の精度対策: ノイズの座標を「整数のマス番号 + マス内の小数」に分けて持つ。
//   マス番号(int2)と小数(float2)は、カメラ位置(ノイズ空間 = ワールド座標 * 倍率)を CPU 側で double から分けた値。
//   視線上の点は「カメラからの相対位置」だけで計算するので、ワールド座標がどれだけ大きくなっても値が小さいまま。
//   層ごとに倍率が違うので、倍率ごとに CPU 側で分けた値を受け取る(gBase / gLowA / gLowB / gFloor)。
//   fbm のオクターブは厳密に 2 倍ずつ(整数の倍なので、マス番号も 2 倍にするだけで厳密に引き継げる)。
//
// 色は、プレビュー(雲海チューナー)で見た値と同じにするため、表示用の値で書いて最後にリニアへ戻す。

cbuffer CloudSeaParam : register(b0) {
    row_major float4x4 gInvViewProj;
    float32_t3 gCameraPosition;
    float32_t gSeaHeight;      // 雲海の平均の高さ(ワールド y)
    float32_t3 gSunDirection;  // 太陽へ向かう向き(正規化済み)
    float32_t gThickness;      // 雲の厚み(下へ)
    int2 gBaseCell;            // 主ノイズ: カメラ位置のマス番号(整数部)
    float32_t2 gBaseFrac;      //            カメラ位置のマス内の位置(小数部、0 以上 1 未満)
    int2 gLowCellA;            // 大きなうねり 1 層目
    float32_t2 gLowFracA;
    int2 gLowCellB;            // 大きなうねり 2 層目
    float32_t2 gLowFracB;
    int2 gFloorCell;           // 切れ間から見える下の雲層
    float32_t2 gFloorFrac;
    float32_t gBump;           // 起伏の高さ(上へ)
    float32_t gScale;          // ノイズの細かさ(ワールド 1 ユニットあたりのマス数)
    float32_t gThreshold;      // 雲になるしきい値(小さいほど雲が多い)
    float32_t gSoftness;       // 縁のやわらかさ
    float32_t gDensity;        // 濃さ(厚みの見え方)
    float32_t gFog;            // 霞(遠くを背景へ溶かす)
    float32_t gSteps;          // ステップ数
    float32_t gJitter;         // 1 ならステップの開始位置を画素ごとにずらす(縞を砂目に変える)
    float32_t gSwellHeight;    // 大きなうねりの高さ
    float32_t gSwellBlend;     // うねりで雲の量をずらす強さ
    float32_t gBillow;         // 細かい凹凸の折り目の強さ
    float32_t gShadowStrength; // 雲同士の影の強さ
    float32_t gShadowLength;   // 影を調べる距離
    float32_t gGapDarkness;    // 切れ間の暗さ
    float32_t gAerial;         // 空気遠近の強さ
    float32_t gContrast;       // コントラスト
    float32_t gLowRatioA;      // うねり 1 層目の倍率(gScale に対する比)。C++ 側の定数と同じ値が入る
    float32_t gLowRatioB;      // うねり 2 層目の倍率
    float32_t gFloorRatio;     // 下の雲層の倍率
    float32_t gPadding;
};

static const uint32_t kCloudSeaOctaves = 4u;
static const float32_t kCloudSeaFbmNormalize = 0.9375f;   // 4 層の最大値(1 - 0.5^4)。0〜1 に正規化する
static const float32_t kCloudSeaMaxSteps = 32.0f;
static const float32_t kCloudSeaMaxTraceDistance = 700.0f; // これより遠くは調べない(端は霞に溶かす)
static const float32_t kCloudSeaMinLookDown = 0.002f;      // 視線の下向き成分がこれより小さい(ほぼ水平)なら雲海は見えない
static const float32_t kCloudSeaNormalEps = 2.0f;          // 法線を求めるためにずらす距離(ワールド単位)
static const float32_t kCloudSeaMinCoverage = 0.001f;      // これ以下の濃さは雲なしとして扱う
static const float32_t kCloudSeaMinDirY = 0.05f;           // 厚みの計算で割る視線の y 成分の下限(水平に近いときの発散を防ぐ)
static const uint32_t kCloudSeaRefineSteps = 3u;           // 当たった区間を 2 分探索で詰める回数
static const float32_t kCloudSeaPathBias = 2.0f;           // 表面の分の最小の通過距離(縁以外で透けないように)
static const float32_t kCloudSeaGamma = 2.2f;              // 表示用の値 -> リニア
static const uint32_t kCloudSeaShadowSteps = 4u;           // 影を調べる回数
static const float32_t kCloudSeaFloorDepth = 25.0f;        // 切れ間から見える下の雲層の、雲の下面からの深さ
static const float32_t kCloudSeaFloorFog = 0.004f;         // 下の雲層を地平線の色へ溶かす強さ
static const float32_t kCloudSeaEdgeFadeStart = 0.55f;     // 調べる距離のこの割合から、端を地平線の色へ溶かし始める
static const float32_t kCloudSeaAerialBoost = 2.0f;        // 空気遠近を最大にしたときの、霞の増え方(倍率)
static const float32_t kCloudSeaHazeCoolMix = 0.4f;        // 霞の色: 青紫と地平線色の混ぜ具合
static const float32_t kCloudSeaHazeWarmMix = 0.6f;        // 霞の色: 太陽側の暖色の最大の混ぜ具合

// 各オクターブにずらす量(マス単位の小さな定数)。倍率が整数だと各層の格子がそろってしまうので、それを崩す
static const float32_t2 kCloudSeaOctaveOffset[4] = {
    float32_t2(0.00f, 0.00f),
    float32_t2(0.37f, 0.71f),
    float32_t2(0.83f, 0.29f),
    float32_t2(0.11f, 0.57f),
};
static const float32_t2 kCloudSeaLowShiftA = float32_t2(3.1f, 1.7f);
static const float32_t2 kCloudSeaLowShiftB = float32_t2(9.2f, 4.4f);
static const float32_t2 kCloudSeaFloorShift = float32_t2(7.0f, 3.0f);

static const float32_t3 kCloudSeaHorizonColor = float32_t3(0.80f, 0.88f, 1.00f);
static const float32_t3 kCloudSeaZenithColor  = float32_t3(0.12f, 0.35f, 0.80f);
static const float32_t3 kCloudSeaLowColor     = float32_t3(0.32f, 0.46f, 0.74f); // 切れ間の下の空気の色
static const float32_t3 kCloudSeaGapDarkColor = float32_t3(0.20f, 0.28f, 0.50f); // 切れ間の暗さを最大にしたときの色
static const float32_t3 kCloudSeaFloorLight   = float32_t3(0.74f, 0.80f, 0.92f); // 下の雲層の明るい部分
static const float32_t3 kCloudSeaSunColor     = float32_t3(1.00f, 0.92f, 0.75f);
static const float32_t3 kCloudSeaLitColor     = float32_t3(1.00f, 0.99f, 0.96f);
static const float32_t3 kCloudSeaShadowColor  = float32_t3(0.46f, 0.56f, 0.76f);
static const float32_t3 kCloudSeaSilverColor  = float32_t3(1.00f, 0.90f, 0.70f); // 太陽の方を見たときの縁の光
static const float32_t3 kCloudSeaHazeCool     = float32_t3(0.60f, 0.72f, 0.96f); // 空気遠近: 遠くの色(青紫寄り)
static const float32_t3 kCloudSeaHazeWarm     = float32_t3(1.00f, 0.88f, 0.74f); // 空気遠近: 太陽側の色

// 格子点の乱数を、5 次の補間で結ぶ(3 次より格子の跡が目立たない)。cell: 格子の整数座標、f: マス内の位置(0〜1)
float32_t CloudSeaValueNoise(int2 cell, float32_t2 f) {
    float32_t2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    float32_t a = Hash21(cell);
    float32_t b = Hash21(cell + int2(1, 0));
    float32_t c = Hash21(cell + int2(0, 1));
    float32_t d = Hash21(cell + int2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// 4 層の fbm(0〜1)。
// local: カメラからの相対位置(ワールド単位の xz)、scaleRatio: gScale に対する倍率
// baseCell / baseFrac: この倍率でのカメラ位置(整数部と小数部)、billow: 2 層目以降の折り目の強さ、shift: 全体のずらし(マス単位)
float32_t CloudSeaFbm(float32_t2 local, float32_t scaleRatio, int2 baseCell, float32_t2 baseFrac, float32_t billow, float32_t2 shift) {
    float32_t2 q = local * (gScale * scaleRatio) + baseFrac + shift; // 小さい値だけで計算する
    float32_t sum = 0.0f;
    float32_t amplitude = 0.5f;
    [unroll]
    for (uint32_t i = 0; i < kCloudSeaOctaves; ++i) {
        float32_t2 shifted = q + kCloudSeaOctaveOffset[i];
        int2 cell = baseCell + int2(floor(shifted)); // 整数同士の足し算なので厳密
        float32_t v = CloudSeaValueNoise(cell, frac(shifted));
        if (i > 0u) {
            v = lerp(v, 1.0f - abs(2.0f * v - 1.0f), billow); // 折り目: 谷が鋭くなり、もこもこした輪郭になる
        }
        sum += amplitude * v;
        q *= 2.0f;        // 2 倍は浮動小数でも厳密
        baseCell *= 2;
        amplitude *= 0.5f;
    }
    return sum / kCloudSeaFbmNormalize;
}

// 大きなうねり(0〜1)。低い周波数の 2 層。倍率は gScale に対する比なので、gScale を変えると一緒に変わる
float32_t CloudSeaLow(float32_t2 local) {
    float32_t2 qa = local * (gScale * gLowRatioA) + gLowFracA + kCloudSeaLowShiftA;
    float32_t2 qb = local * (gScale * gLowRatioB) + gLowFracB + kCloudSeaLowShiftB;
    float32_t a = CloudSeaValueNoise(gLowCellA + int2(floor(qa)), frac(qa));
    float32_t b = CloudSeaValueNoise(gLowCellB + int2(floor(qb)), frac(qb));
    return 0.65f * a + 0.35f * b;
}

// 位置 local(カメラからの相対 xz)での雲の柱。x = 濃さ(0〜1、0 は雲なし)、y = 上面の高さ(雲海の平均の高さが 0)
float32_t2 CloudSeaField(float32_t2 local) {
    float32_t n = CloudSeaFbm(local, 1.0f, gBaseCell, gBaseFrac, gBillow, float32_t2(0.0f, 0.0f));
    float32_t swell = CloudSeaLow(local);
    n += gSwellBlend * (swell - 0.5f); // うねりの高い所に雲を集める
    float32_t cover = smoothstep(gThreshold, gThreshold + gSoftness, n);
    return float32_t2(cover, cover * (gBump + gSwellHeight * swell));
}

// 高さ y の点が、柱 field(x = 濃さ、y = 上面の高さ)の内側か
bool CloudSeaInside(float32_t y, float32_t2 field) {
    return field.x > kCloudSeaMinCoverage && y < field.y && y > -gThickness * field.x;
}

// 雲に隠れていない部分の背景。上は空、下は切れ間から見える遠い雲層(空色の穴に見えないよう、暗い青灰 + 薄い雲)
float32_t3 CloudSeaBackground(float32_t3 dir, float32_t3 sunDir, float32_t originY) {
    float32_t3 bg;
    if (dir.y >= 0.0f) {
        bg = lerp(kCloudSeaHorizonColor, kCloudSeaZenithColor, pow(saturate(dir.y), 0.5f));
    } else {
        float32_t t = (-gThickness - kCloudSeaFloorDepth - originY) / min(dir.y, -kCloudSeaMinLookDown);
        float32_t n = CloudSeaFbm(dir.xz * t, gFloorRatio, gFloorCell, gFloorFrac, 0.0f, kCloudSeaFloorShift);
        float32_t3 gap = lerp(kCloudSeaLowColor, kCloudSeaGapDarkColor, gGapDarkness);
        float32_t3 floorColor = lerp(gap, kCloudSeaFloorLight, smoothstep(0.38f, 0.72f, n) * 0.8f);
        bg = lerp(floorColor, kCloudSeaHorizonColor, 1.0f - exp(-t * kCloudSeaFloorFog));
    }
    float32_t cosSun = saturate(dot(dir, sunDir));
    bg += kCloudSeaSunColor * (pow(cosSun, 48.0f) * 0.5f + smoothstep(0.9990f, 0.9996f, cosSun) + pow(cosSun, 8.0f) * 0.10f);
    return bg;
}

// 雲海つきの色(表示用の値)。dir: 視線方向、pixel: 画素の座標(ジッタの乱数用)
float32_t3 CloudSeaColor(float32_t3 dir, float32_t3 sunDir, float32_t2 pixel) {
    float32_t originY = gCameraPosition.y - gSeaHeight;
    float32_t3 bg = CloudSeaBackground(dir, sunDir, originY);
    float32_t3 result = bg;

    if (dir.y < -kCloudSeaMinLookDown) {
        // 雲の柱が存在しうる高さの範囲 [-gThickness, hMax] と、視線が交わる区間 [tTop, tBottom]
        float32_t hMax = gBump + gSwellHeight;
        float32_t tTop = max((hMax - originY) / dir.y, 0.0f);
        float32_t tBottom = min((-gThickness - originY) / dir.y, tTop + kCloudSeaMaxTraceDistance);

        if (tBottom > tTop) {
            // 区間を等間隔に刻み、最初に雲の柱の内側に入ったステップを探す
            float32_t steps = clamp(gSteps, 1.0f, kCloudSeaMaxSteps);
            float32_t dt = (tBottom - tTop) / steps;
            float32_t t = tTop + Hash21(int2(pixel)) * dt * gJitter;
            float32_t prevT = tTop;
            float32_t2 field = float32_t2(0.0f, 0.0f);
            bool hit = false;
            [loop]
            for (float32_t i = 0.0f; i < steps; i += 1.0f) {
                field = CloudSeaField(dir.xz * t);
                if (CloudSeaInside(originY + dir.y * t, field)) {
                    hit = true;
                    break;
                }
                prevT = t;
                t += dt;
            }

            if (hit) {
                // 前のステップ(外)と当たったステップ(内)の間を 2 分探索で詰めて、面の位置を正確にする(段状の縞を消す)
                float32_t lo = prevT;
                float32_t hi = t;
                [unroll]
                for (uint32_t k = 0; k < kCloudSeaRefineSteps; ++k) {
                    float32_t mid = 0.5f * (lo + hi);
                    float32_t2 fieldMid = CloudSeaField(dir.xz * mid);
                    if (CloudSeaInside(originY + dir.y * mid, fieldMid)) {
                        hi = mid;
                        field = fieldMid;
                    } else {
                        lo = mid;
                    }
                }
                t = hi;
                float32_t2 local = dir.xz * t;
                float32_t y = originY + dir.y * t;

                // 面の向き(高さの勾配)から陰影をつける
                float32_t eps = kCloudSeaNormalEps;
                float32_t h0 = field.y;
                float32_t hx = CloudSeaField(local + float32_t2(eps, 0.0f)).y;
                float32_t hz = CloudSeaField(local + float32_t2(0.0f, eps)).y;
                float32_t3 normal = normalize(float32_t3(-(hx - h0) / eps, 1.0f, -(hz - h0) / eps));
                float32_t diffuse = saturate(dot(normal, sunDir) * 0.5f + 0.5f);

                // 柱の中の高さの割合(下面 0、上面 1)。上ほど明るい
                float32_t columnHeight = h0 + gThickness * field.x + 0.001f;
                float32_t heightRatio = saturate((y + gThickness * field.x) / columnHeight);
                float32_t lit = diffuse * 0.65f + heightRatio * 0.35f;

                // 雲同士の影: 面の点から太陽方向へ進み、手前の雲の上面より低ければ、日陰
                float32_t shade = 1.0f;
                if (gShadowStrength > kCloudSeaMinCoverage) {
                    float32_t occlusion = 0.0f;
                    float32_t softRange = 0.5f * gBump + 0.5f * gSwellHeight + 1.0f; // 高さの差をこの幅で 0〜1 にする
                    for (uint32_t s = 1u; s <= kCloudSeaShadowSteps; ++s) {
                        float32_t distance = gShadowLength * float32_t(s) / float32_t(kCloudSeaShadowSteps);
                        float32_t3 q = float32_t3(local.x, h0, local.y) + sunDir * distance;
                        float32_t topAtQ = CloudSeaField(q.xz).y;
                        occlusion = max(occlusion, (topAtQ - q.y) / softRange);
                    }
                    shade = 1.0f - saturate(occlusion) * gShadowStrength;
                }
                float32_t3 cloud = lerp(kCloudSeaShadowColor, kCloudSeaLitColor, lit * shade);

                // 太陽の方を見たとき、縁(濃さが小さい所)が光る
                float32_t cosSun = saturate(dot(dir, sunDir));
                cloud += kCloudSeaSilverColor * pow(cosSun, 6.0f) * (1.0f - field.x) * 0.5f;

                // 厚み: 見つけた点から柱の下面までの視線方向の距離を、濃さで透過率へ換算する
                float32_t path = (y + gThickness * field.x) / max(-dir.y, kCloudSeaMinDirY);
                float32_t alpha = 1.0f - exp(-gDensity * (path + kCloudSeaPathBias));

                // 空気遠近: 遠いほど青紫へ、太陽側ほど暖色へ溶かす。調べる距離の端は地平線の色へ溶かし、境目を消す
                float32_t3 haze = lerp(lerp(kCloudSeaHazeCool, kCloudSeaHorizonColor, kCloudSeaHazeCoolMix),
                                       kCloudSeaHazeWarm, pow(cosSun, 4.0f) * kCloudSeaHazeWarmMix);
                float32_t fog = 1.0f - exp(-t * gFog * (1.0f + gAerial * kCloudSeaAerialBoost));
                cloud = lerp(cloud, haze, fog);
                float32_t edge = smoothstep(kCloudSeaEdgeFadeStart, 1.0f, (t - tTop) / kCloudSeaMaxTraceDistance);
                cloud = lerp(cloud, kCloudSeaHorizonColor, edge);

                result = lerp(bg, cloud, alpha);
            }
        }
    }

    // コントラスト: S 字の曲線を、強さに応じて混ぜる
    result = saturate(result); // 曲線は 0〜1 の範囲で定義されているので、先に収める
    float32_t3 curved = result * result * (3.0f - 2.0f * result);
    return lerp(result, curved, gContrast);
}

#endif
