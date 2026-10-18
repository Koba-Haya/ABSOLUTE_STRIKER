#ifndef VOLUME_CLOUD_HLSLI
#define VOLUME_CLOUD_HLSLI

// ボリューム雲(3D ノイズ + レイマーチ)。外形は 2 種類(gShapeMode):
//   箱: ワールドに固定した箱の中に、雲を 1 つ浮かべる(箱に内接する楕円体の塊)
//   層: 下面と上面の高さだけで決まる、水平に無限に続く雲の層(雲海。上から見下ろす)。
//       水平はノイズが繰り返す。歩幅は入口の近くを細かくし、遠くは霞へ溶かす。
//
// 仕組み(Horizon Zero Dawn と同じ考え方):
//   1. 視線と外形の範囲(入口 tNear と出口 tFar)を求める。範囲の外の画素は、ここで終わる(軽い)。
//   2. 入口から出口まで、一定の歩幅で進む(レイマーチ)。シーンの深度(物体までの距離)で打ち切るので、
//      物体の手前にある雲だけが、物体に重なる = 雲の中の敵や弾が霞んで見える。
//   3. 各点で雲の濃さ(密度)を 3D ノイズから求める。
//        形(shape): R = Perlin-Worley(大まかな形)。G/B/A の Worley で削って、もこもこした輪郭にする。
//        細かい模様(detail): 3 つの Worley で、さらに細かく削る。
//        箱の縁は薄くして(BoxFade)、箱の形が見えないようにする。
//   4. 光を求める。太陽方向へ少数の点で濃さを調べ(LightTransmittance)、ビアの法則で届く光を求める。
//      散乱の向き(位相関数)は Henyey-Greenstein。太陽の方を見ると雲が明るく光る(前方散乱)。
//   5. 視線に沿って、透過率(光がどれだけ通り抜けるか)と、雲が足す光を積算する。
//      出力は「雲の光 + (1 - 透過率)」。ブレンドで、元の画面 * 透過率 + 雲の光 になる(プリマルチプライド)。
//
// 色はリニアで書く(描画先が sRGB のため、ハードウェアが表示用へ変換する)。
//
// 雲は、シーンより低い解像度で描く(VolumeCloud.PS)。深度を縮める(VolumeCloudDepth.PS)のと、
// 元の解像度へ拡大して重ねる(VolumeCloudComposite.PS)のは、別のパス。3 つのパスは VolumeCloudParam.hlsli の定数を共有する。

#include "VolumeCloudParam.hlsli"

Texture3D<float32_t4> gShape : register(t0);   // 形のノイズ(R: Perlin-Worley, G/B/A: Worley)
Texture3D<float32_t4> gDetail : register(t1);  // 細かい模様(R/G/B: Worley)
Texture2D<float32_t> gSceneDistance : register(t2); // カメラから物体までの距離(パス 1 で、低解像度へ縮めた結果)
SamplerState gWrap : register(s1);             // 線形補間 + 繰り返し(ノイズはタイル可能)

static const uint32_t kMaxSteps = 64u;                 // gSteps の上限(ループを抑える)
static const uint32_t kMaxLightSteps = 8u;             // gLightSteps の上限
static const float32_t kMinDirComponent = 1.0e-6f;     // 視線の成分がこれより小さいときの置き換え(0 割り回避)
static const float32_t kMinRange = 1.0e-4f;            // Remap で割る幅の下限
static const float32_t kMinEdgeFade = 1.0e-3f;
static const float32_t kMinDensity = 0.001f;           // これ以下の密度は、光の計算を飛ばす
static const float32_t kOpaqueCutoff = 0.01f;          // 透過率がこれ以下になったら、積算を打ち切る
static const float32_t kLightStepGrowth = 1.5f;        // 太陽方向の歩幅を、1 歩ごとに伸ばす倍率(遠くまで見る)
static const float32_t kFbmWeightA = 0.625f;           // Worley 3 つを fbm にまとめる重み(合計 1)
static const float32_t kFbmWeightB = 0.25f;
static const float32_t kFbmWeightC = 0.125f;
static const float32_t kBackLobeRatio = 0.5f;          // 後ろ向きの散乱の強さ(前向きに対する比)
static const float32_t kForwardWeight = 0.7f;          // 位相関数: 前向きの山の割合(残りは後ろ向き)
static const float32_t kMinPhaseDenominator = 1.0e-3f;
static const float32_t kPhaseExponent = 1.5f;
static const float32_t2 kInterleavedGradientScale = float32_t2(0.06711056f, 0.00583715f); // 画素ごとの乱数(縞を砂目にする)
static const float32_t kInterleavedGradientMul = 52.9829189f;

static const float32_t kShapeModeLayerThreshold = 0.5f; // gShapeMode はこの値より大きければ層(float で受けるので、整数との比較に幅を持たせる)
static const float32_t kLayerMarchTopRatio = 0.8f;     // 層: 視線を進め始める高さ(下面から上面までの割合)。上面近くは、薄くて実際には雲がほぼ無いので、そこを飛ばして、雲の表面に近い所から細かい歩幅で進む
static const float32_t kLayerBottomFade = 0.15f;       // 層: 下面から、この高さの割合までで濃くなる(下面は平らに近い)
static const float32_t kLayerStepPower = 2.0f;         // 層: 歩幅の伸び方(大きいほど、入口の近くが細かく、遠くが粗い)
static const float32_t kLayerMaxPathLength = 300.0f;   // 層: 1 本の視線が層の中を進む長さの上限(水平に近い視線は、層の中を長く進むが、それより先は光が尽きていて見えない。歩幅を細かく保つ)
static const float32_t kLayerJitterMin = 0.30f;        // 層: 区間の中でのずらしの範囲(0〜1 全部だと粒が目立つので、中央寄りに狭める。縞と粒の兼ね合い)
static const float32_t kLayerJitterRange = 0.4f;
static const float32_t kLayerEdgeSharpness = 0.5f;     // 層: 雲の縁の急さ(小さいほど急で、輪郭がくっきりする。大きいと、ぼやける)
static const float32_t kLayerEdgeFadeStart = 0.75f;    // 層: 描く距離のこの割合から、雲を薄くして霞へ溶かし始める
static const float32_t3 kLayerHazeColor = float32_t3(0.612f, 0.755f, 1.000f); // 層: 遠くの霞の色(リニア。旧方式の雲海の地平線の色)
// 層は水平に無限に続くので、ノイズのタイルが繰り返して見える(遠くでは縞になる)。次の 2 つで崩す。
//   大きな周期の濃淡: タイルと割り切れない大きさの周期で、雲の量をゆるやかに変える(周期が違う 2 つが重なり、繰り返しが目立たない)
//   遠くの細かさの省略: 遠いほど、1 画素が広い範囲を覆うので、細かい模様を平均へ寄せる(粒や縞になるのを防ぐ)
static const float32_t kLayerCoverageScale = 0.137f;     // 大きな周期の濃淡: 形のノイズの細かさに対する倍率(1 以下の、割り切れない値)
static const float32_t kLayerCoverageSlice = 0.37f;      // 大きな周期の濃淡: ノイズを引く高さ(ノイズ座標)
static const float32_t kLayerCoverageStrength = 0.40f;   // 大きな周期の濃淡: 最も薄い所で、雲の量をどこまで減らすか(割合)
static const float32_t kCoverageLow = 0.55f;             // 形のノイズ R の値を 0〜1 にならす範囲
static const float32_t kCoverageHigh = 0.85f;
static const float32_t kLayerLodStartRatio = 0.35f;      // 細かい模様を省き始める距離(描く距離の上限に対する割合)
static const float32_t kLayerLodEndRatio = 0.90f;        // 細かい模様を完全に省く距離
static const float32_t kLowFbmMean = 0.476f;             // 形のノイズ G/B/A の fbm の平均(生成したノイズの統計。省いたときの寄せ先)

static const float32_t3 kSunColor = float32_t3(1.00f, 0.93f, 0.80f);
static const float32_t3 kAmbientLow = float32_t3(0.20f, 0.27f, 0.42f);   // 箱の下側の、空からの光
static const float32_t3 kAmbientHigh = float32_t3(0.55f, 0.62f, 0.78f);  // 箱の上側
// 層(雲海)の、空からの光。影の青みを、旧方式の雲海の影の色(表示用 0.46, 0.56, 0.76)に合わせる(暗い灰色にならないように)
static const float32_t3 kLayerAmbientLow = float32_t3(0.20f, 0.30f, 0.56f);   // 層の下側(影)
static const float32_t3 kLayerAmbientHigh = float32_t3(0.62f, 0.72f, 0.92f);  // 層の上側

float32_t Remap(float32_t v, float32_t oldMin, float32_t oldMax, float32_t newMin, float32_t newMax) {
    return newMin + (v - oldMin) / max(oldMax - oldMin, kMinRange) * (newMax - newMin);
}

// 画素ごとの乱数(0〜1)。レイの開始位置をずらして、ステップの段状の縞を細かい粒に変える
float32_t InterleavedGradientNoise(float32_t2 pixel) {
    return frac(kInterleavedGradientMul * frac(dot(pixel, kInterleavedGradientScale)));
}

// 視線と箱の交差。交差すれば true で、tNear / tFar に入口と出口の距離(カメラから)を返す
bool RayBox(float32_t3 origin, float32_t3 dir, out float32_t tNear, out float32_t tFar) {
    float32_t3 safeDir = select(abs(dir) < kMinDirComponent, float32_t3(kMinDirComponent, kMinDirComponent, kMinDirComponent), dir);
    float32_t3 invDir = 1.0f / safeDir;
    float32_t3 t0 = (gBoxCenter - gBoxHalfSize - origin) * invDir;
    float32_t3 t1 = (gBoxCenter + gBoxHalfSize - origin) * invDir;
    float32_t3 tMin = min(t0, t1);
    float32_t3 tMax = max(t0, t1);
    tNear = max(max(tMin.x, tMin.y), tMin.z);
    tFar = min(min(tMax.x, tMax.y), tMax.z);
    return tFar > max(tNear, 0.0f);
}

bool IsLayer() {
    return gShapeMode > kShapeModeLayerThreshold;
}

// 視線と層(下面と上面の間)の交差。交差すれば true で、tNear / tFar に入口と出口の距離(カメラから)を返す。
// 水平に近い視線は、層の中にいるときだけ、距離の上限まで続く
bool RaySlab(float32_t3 origin, float32_t3 dir, out float32_t tNear, out float32_t tFar) {
    tNear = 0.0f;
    tFar = gMaxDistance;
    if (abs(dir.y) < kMinDirComponent) {
        return origin.y >= gLayerBottom && origin.y <= gLayerBottom + (gLayerTop - gLayerBottom) * kLayerMarchTopRatio;
    }
    float32_t marchTop = gLayerBottom + (gLayerTop - gLayerBottom) * kLayerMarchTopRatio;
    float32_t t0 = (gLayerBottom - origin.y) / dir.y;
    float32_t t1 = (marchTop - origin.y) / dir.y;
    tNear = min(t0, t1);
    tFar = max(t0, t1);
    return tFar > max(tNear, 0.0f);
}

// 層の高さ方向の重み(0〜1)。下面から少し上で濃くなり(下面は平らに近い)、上面へ向けて薄くなる(edgeFade の幅)。
// 上面は、ノイズの値で削られる位置が変わるので、起伏のある雲の頂になる
float32_t LayerFade(float32_t3 p) {
    float32_t h = (p.y - gLayerBottom) / max(gLayerTop - gLayerBottom, kMinRange); // 下面 0、上面 1
    if (h <= 0.0f || h >= 1.0f) {
        return 0.0f;
    }
    float32_t rise = smoothstep(0.0f, kLayerBottomFade, h);
    float32_t fall = 1.0f - smoothstep(1.0f - max(gEdgeFade, kMinEdgeFade), 1.0f, h);
    return rise * fall;
}

// 雲の外形の縁へ向けて 1 -> 0 になる値。箱に内接する楕円体を外形にして、その縁を薄くする
// (箱の面や角がそのまま雲の輪郭になると、直方体に見えてしまうため)。
// 楕円体の表面で 0、中心側(縁から edgeFade の幅より内側)で 1。箱の角は、外形の外なので 0
float32_t BoxFade(float32_t3 p) {
    float32_t radius = length((p - gBoxCenter) / gBoxHalfSize); // 楕円体の表面で 1、中心で 0
    return 1.0f - smoothstep(1.0f - max(gEdgeFade, kMinEdgeFade), 1.0f, radius);
}

// 点 p の雲の濃さ(0〜1)。useDetail: 細かい模様でも削る(光の計算では、軽くするために使わない)。
// lod: 細かさを省く割合(0 = そのまま、1 = 細かい模様を全部省く)。遠くの層で、粒や縞になるのを防ぐ
float32_t SampleDensity(float32_t3 p, bool useDetail, float32_t lod) {
    bool layer = IsLayer();
    float32_t fade = layer ? LayerFade(p) : BoxFade(p);
    if (fade <= 0.0f) {
        return 0.0f;
    }

    // ノイズの座標へ(風で流れる)。箱は中心を基準にして、ノイズが箱に付く。層はワールド座標のまま(水平に繰り返す)
    float32_t3 local = layer ? p : p - gBoxCenter;
    float32_t3 noisePos = (local - gWindVelocity * gTime) * gNoiseScale;
    float32_t4 shape = gShape.SampleLevel(gWrap, noisePos, 0.0f);

    if (layer) {
        // 大きな周期の濃淡(繰り返し模様を崩す)
        float32_t3 coveragePos = float32_t3((local.xz - gWindVelocity.xz * gTime) * (gNoiseScale * kLayerCoverageScale), kLayerCoverageSlice);
        float32_t coverage = gShape.SampleLevel(gWrap, coveragePos, 0.0f).r;
        fade *= lerp(1.0f - kLayerCoverageStrength, 1.0f, smoothstep(kCoverageLow, kCoverageHigh, coverage));
    }

    // 形: Perlin-Worley の起伏を、細かさの違う Worley の fbm で削る(遠くでは、fbm を平均へ寄せる)
    float32_t lowFbm = shape.g * kFbmWeightA + shape.b * kFbmWeightB + shape.a * kFbmWeightC;
    lowFbm = lerp(lowFbm, kLowFbmMean, lod);
    float32_t base = saturate(Remap(shape.r, lowFbm - 1.0f, 1.0f, 0.0f, 1.0f));

    // 縁を薄くしてから、しきい値より小さい所を雲なしにする(縁ほど隙間が増え、塊の輪郭になる)
    float32_t cloud = saturate(Remap(base * fade, gThreshold, 1.0f, 0.0f, 1.0f));

    if (useDetail && cloud > 0.0f && lod < 1.0f) {
        float32_t3 detail = gDetail.SampleLevel(gWrap, noisePos * gDetailScale, 0.0f).rgb;
        float32_t highFbm = detail.r * kFbmWeightA + detail.g * kFbmWeightB + detail.b * kFbmWeightC;
        cloud = saturate(Remap(cloud, highFbm * gErosion * (1.0f - lod), 1.0f, 0.0f, 1.0f));
    }
    if (layer) {
        // 縁を急にして、輪郭と起伏の陰影をくっきりさせる(ゆるい勾配のままだと、全体がぼやけて見える)
        cloud = smoothstep(0.0f, kLayerEdgeSharpness, cloud);
    }
    return cloud;
}

// Henyey-Greenstein 位相関数。等方(g = 0)のとき 1 になるように正規化してある(4π を掛けた形)
float32_t HenyeyGreenstein(float32_t cosTheta, float32_t g) {
    float32_t g2 = g * g;
    return (1.0f - g2) / pow(max(1.0f + g2 - 2.0f * g * cosTheta, kMinPhaseDenominator), kPhaseExponent);
}

// 前向き(太陽の方を見たとき強い)と後ろ向きの 2 つの山を混ぜる
float32_t Phase(float32_t cosTheta) {
    return lerp(HenyeyGreenstein(cosTheta, -gPhaseG * kBackLobeRatio), HenyeyGreenstein(cosTheta, gPhaseG), kForwardWeight);
}

// 点 p から太陽方向へ進み、途中の雲に遮られずに届く光の割合(ビアの法則)
float32_t LightTransmittance(float32_t3 p, float32_t lod) {
    uint32_t count = (uint32_t)clamp(gLightSteps, 0.0f, (float32_t)kMaxLightSteps);
    float32_t stepLength = gLightStepLength;
    float32_t opticalDepth = 0.0f;
    [loop]
    for (uint32_t i = 0u; i < count; ++i) {
        p += gSunDirection * stepLength;
        opticalDepth += SampleDensity(p, false, lod) * stepLength;
        stepLength *= kLightStepGrowth;
    }
    return exp(-opticalDepth * gExtinction);
}

struct VolumeCloudResult {
    float32_t3 color;          // 雲が足す光
    float32_t transmittance;   // 雲を通り抜ける光の割合(1 = 雲なし)
};

// 視線に沿って雲を積算する。sceneDistance: カメラから物体までの距離(何もなければ無限遠)、pixel: 画素の座標
VolumeCloudResult MarchVolumeCloud(float32_t3 origin, float32_t3 dir, float32_t sceneDistance, float32_t2 pixel) {
    VolumeCloudResult result;
    result.color = float32_t3(0.0f, 0.0f, 0.0f);
    result.transmittance = 1.0f;

    // 視線が通る範囲(入口と出口)。箱は箱の中、層は下面と上面の間
    bool layer = IsLayer();
    float32_t tNear;
    float32_t tFar;
    bool hit = layer ? RaySlab(origin, dir, tNear, tFar) : RayBox(origin, dir, tNear, tFar);
    if (!hit) {
        return result;
    }
    tNear = max(tNear, 0.0f);
    tFar = min(tFar, sceneDistance); // 物体の向こう側の雲は見えない
    if (layer) {
        tFar = min(tFar, gMaxDistance); // 層は、描く距離の上限で打ち切る(その先は霞)
        tFar = min(tFar, tNear + kLayerMaxPathLength);
    }
    if (tFar <= tNear) {
        return result;
    }

    // 歩幅の配分: 箱は等間隔。層は、入口の近くを細かく、遠くを粗くする(上から見ると、光が近くで尽きるため。
    // 区間を u = 0〜1 に分け、位置を u の累乗で決める)。各区間の中で、画素ごとにずらして縞を粒に変える
    float32_t steps = clamp(gSteps, 1.0f, (float32_t)kMaxSteps);
    float32_t stepPower = layer ? kLayerStepPower : 1.0f;
    float32_t jitter = InterleavedGradientNoise(pixel);
    float32_t marchLength = tFar - tNear;
    float32_t phase = Phase(dot(dir, gSunDirection));

    [loop]
    for (float32_t i = 0.0f; i < steps; i += 1.0f) {
        float32_t tStart = tNear + marchLength * pow(i / steps, stepPower);
        float32_t tEnd = tNear + marchLength * pow((i + 1.0f) / steps, stepPower);
        float32_t dt = tEnd - tStart;
        float32_t t = lerp(tStart, tEnd, layer ? kLayerJitterMin + jitter * kLayerJitterRange : jitter);

        float32_t3 p = origin + dir * t;
        // 遠いほど細かい模様を省く(層のみ)
        float32_t lod = layer ? smoothstep(kLayerLodStartRatio * gMaxDistance, kLayerLodEndRatio * gMaxDistance, t) : 0.0f;
        float32_t density = SampleDensity(p, true, lod);
        if (layer) {
            // 描く距離の端へ向けて薄くする(境目を消す)
            density *= 1.0f - smoothstep(kLayerEdgeFadeStart * gMaxDistance, gMaxDistance, t);
        }
        if (density > kMinDensity) {
            float32_t stepTransmittance = exp(-density * gExtinction * dt);

            // 光: 太陽(遮られずに届く分 * 位相)と、空からの光(上ほど明るい)
            float32_t heightRatio = layer ? saturate((p.y - gLayerBottom) / max(gLayerTop - gLayerBottom, kMinRange))
                                          : saturate(0.5f + (p.y - gBoxCenter.y) / (2.0f * gBoxHalfSize.y));
            float32_t3 ambient = (layer ? lerp(kLayerAmbientLow, kLayerAmbientHigh, heightRatio) : lerp(kAmbientLow, kAmbientHigh, heightRatio)) * gAmbientIntensity;
            float32_t3 sun = kSunColor * gSunIntensity * LightTransmittance(p, lod) * phase;
            float32_t3 scatter = ambient + sun;
            if (layer) {
                // 遠いほど、地平線の色へ溶かす(空気遠近)
                scatter = lerp(scatter, kLayerHazeColor, 1.0f - exp(-t * gFogDensity));
            }

            // この歩幅で雲が足す光は、(1 - 歩幅の透過率) に比例する。手前の雲を通り抜けた分だけ届く
            result.color += result.transmittance * (1.0f - stepTransmittance) * scatter;
            result.transmittance *= stepTransmittance;
            if (result.transmittance < kOpaqueCutoff) {
                break;
            }
        }
    }
    return result;
}

#endif
