#include "Fullscreen.hlsli"
#include "Noise.hlsli"
#include "LensMist.hlsli"

#include "TransitionParam.hlsli"

// 画面遷移の最終パスの 1 つ目。ポストエフェクトのあと、画面全体にかける(定数バッファは TransitionParam.hlsli)。
// 霧の上にのる水滴は、2 つ目のパス(LensDrops.PS.hlsl)が描く。

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepth : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t kKindFade = 0.0f;
static const float32_t kKindNoiseWipe = 1.0f;
static const float32_t kKindZoomBlur = 2.0f;
static const float32_t kKindRipple = 3.0f;
static const float32_t kKindDroplets = 4.0f;
static const float32_t kKindCloudDive = 5.0f;
static const float32_t kKindMistLens = 6.0f;
static const float32_t kMistDebugViewThreshold = 0.5f; // デバッグ表示の番号がこれを超えたら、距離の表示にする
static const float32_t kMistPlaneEnabledThreshold = 0.5f; // gMistPlane.y がこれを超えたら、水平面を使う
static const float32_t kMistDebugFarTint = 0.6f;        // 深度の無い画素の、緑・青の割合(赤みをつける)

static const float32_t kKindTolerance = 0.5f;          // gKind は float で受けるので、整数との比較に幅を持たせる

static const float32_t kPi = 3.14159265f;
static const float32_t kTwoPi = 6.2831853f;
static const float32_t kMinLength = 0.0001f;
static const float32_t kMinSoftness = 0.0001f;

// ノイズ
static const uint32_t kNoiseOctaves = 4u;
static const float32_t kNoiseOctaveNormalize = 0.9375f; // 4 層の最大値(1 - 0.5^4)。0〜1 に正規化する

// ズームブラー
static const uint32_t kZoomBlurTaps = 12u;
static const float32_t kZoomBlurFadeStart = 0.35f;     // 覆い具合がこの値を超えたあたりから、単色へ溶かし始める

// 水の波紋(中心から外へ広がる円形の波。波の先頭より内側を単色で覆う)
static const float32_t kRippleMaxRadius = 1.2f;        // 覆い具合 1 のときの波の先頭の半径(画面の高さを 1 とする)
static const float32_t kRippleFrequency = 38.0f;       // 波の細かさ
static const float32_t kRippleDecay = 9.0f;            // 波の先頭から離れるほど弱まる速さ
static const float32_t kRippleAmplitude = 0.035f;      // 画面を歪ませる最大の量(UV)
static const float32_t kRippleEdgeWidth = 0.12f;       // 覆う範囲の縁の幅(画面の高さを 1 とする)

// レンズの水滴: くもったガラスの上を、水滴が流れる。
//   背景: ぼかした画面(くもり)。水滴の中: 像が上下反転した、くっきりした画面(レンズ)。
//   流れた跡: くもりが晴れて背景が見える。水滴ごとに、縁を暗く、上側にハイライト
static const uint32_t kDropletLayers = 2u;             // 大きさの違う水滴の層の数
static const float32_t kDropletScaleSmall = 4.5f;      // 1 層目の、画面の高さあたりのマス数
static const float32_t kDropletScaleLarge = 8.5f;      // 最後の層の、画面の高さあたりのマス数
static const int32_t kDropletSeedBase = 3;             // 層ごとの乱数の種(整数)
static const int32_t kDropletSeedStep = 11;
static const float32_t kDropletFlowSpeedBase = 0.35f;  // 水滴が流れる速さ(マス / 秒)
static const float32_t kDropletFlowSpeedVariance = 0.25f;
static const float32_t kDropletAppearSpread = 0.7f;    // 水滴ごとの現れる時期のばらつき(覆い具合の割合)
static const float32_t kDropletAppearLength = 0.25f;   // 水滴が現れてから最大になるまでの長さ(覆い具合の割合)
static const float32_t kDropletRadiusBase = 0.13f;     // 水滴の半径(マスを 1 とする)
static const float32_t kDropletRadiusVariance = 0.11f;
static const float32_t kDropletOffsetX = 0.45f;        // 水滴の中心の横のずらし幅(マス単位)
static const float32_t kDropletCenterYBase = 0.38f;    // 水滴の中心の縦の位置(マス単位)
static const float32_t kDropletCenterYVariance = 0.24f;
static const float32_t kDropletStretch = 0.82f;        // 縦に細長くする度合い(1 で円)
static const float32_t kDropletTrailChance = 0.45f;    // この値より大きい乱数を持つ水滴だけ、跡を引く
static const float32_t kDropletTrailWidth = 0.5f;      // 跡の幅(水滴の半径に対する割合)
static const float32_t kDropletTrailTipWidth = 0.25f;  // 跡の先端の幅(根元に対する割合)
static const float32_t kDropletTrailStrength = 0.85f;
static const float32_t kDropletLensMagnify = 0.95f;    // 水滴の中の像の、反転・拡大の度合い
static const float32_t kDropletRimStart = 0.55f;       // 縁が暗くなり始める位置(水滴の半径に対する割合)
static const float32_t kDropletRimDarken = 0.45f;
static const float32_t2 kDropletHighlightPos = float32_t2(-0.35f, -0.42f); // ハイライトの位置(水滴の中心からの割合)
static const float32_t kDropletHighlightSize = 0.42f;
static const float32_t kDropletHighlightGain = 0.85f;
static const float32_t kDropletBlurBase = 0.004f;      // くもり(ぼかし)の半径の最小(画面の高さを 1 とする)
static const float32_t kDropletBlurGain = 0.020f;      // 覆い具合に応じて増えるぼかしの半径
static const float32_t kDropletTrailClearBlur = 0.35f; // 跡の中のぼかしの割合
static const float32_t kDropletTint = 0.5f;            // 覆い具合に応じた、全体の青みと暗さ
static const float32_t3 kDropletTintColor = float32_t3(0.92f, 0.95f, 1.0f);
static const uint32_t kDropletBlurTaps = 12u;
static const float32_t kGoldenAngle = 2.39996f;        // ぼかしのサンプルを散らす角度(黄金角)
static const float32_t kDropletWhiteStart = 0.7f;      // 覆い具合がこの値を超えたあたりから、完全に単色へ溶かす

// 雲を抜ける(雲突入): 世界に固定された雲の層(高さ gSlabBottom 〜 gSlabTop)の中を、実際のカメラと自機が通る。
//   雲は視線ごとのレイマーチ(体積の積算)で、光は太陽方向へ 1 回だけ濃さを調べて求める。
//   シーンの深度で打ち切るので、層の中の自機は「自機の手前の雲」だけをかぶり、霞んで見える。
//   雲の厚み(光学的な深さ)で画面を覆うので、白で塗りつぶさない。
//   レンズ: 雲は水なので、水の膜が付いて流れる(前進の反対向きへ。歪み、色収差)。抜ける頃にレンズフレアとダスト
static const uint32_t kCloudSteps = 24u;               // レイマーチのステップ数
static const float32_t kCloudMaxMarch = 520.0f;        // 1 本の視線で調べる最大の距離
static const float32_t3 kCloudNoiseScale = float32_t3(0.030f, 0.040f, 0.030f); // 雲のノイズの細かさ(ワールド 1 ユニットあたり)
static const float32_t kCloudDensityThreshold = 0.40f; // これより小さいノイズは雲にならない(隙間)
static const float32_t kCloudDensityGain = 0.95f;
static const float32_t kCloudProfileRiseEnd = 0.22f;   // 層の下端から、この高さの割合までで濃くなる
static const float32_t kCloudProfileFallStart = 0.70f; // 層の上端へ向けて、この割合から薄くなる
static const float32_t kCloudBoostBase = 0.8f;         // 層全体の濃さの倍率(遷移の進行に対して、中盤に山を作る)
static const float32_t kCloudBoostPeak = 0.9f;
static const float32_t kCloudBoostCenter = 0.5f;
static const float32_t kCloudBoostWidth = 0.19f;
static const float32_t kCloudAbsorption = 0.55f;       // 濃さ * 距離 から、光を遮る量への換算
static const float32_t kCloudMinDensity = 0.004f;      // これ以下は、光の計算を飛ばす
static const float32_t kCloudOpaqueCutoff = 0.01f;     // 透過率がこれ以下になったら、マーチを打ち切る
static const float32_t kCloudLightStep = 10.0f;        // 太陽方向へ濃さを調べる距離
static const float32_t kCloudLightFalloff = 2.3f;
static const float32_t3 kCloudAmbientLow = float32_t3(0.58f, 0.66f, 0.82f);  // 層の下側の、空からの光
static const float32_t3 kCloudAmbientHigh = float32_t3(0.90f, 0.94f, 1.00f); // 層の上側の、空からの光
static const float32_t3 kCloudSunLight = float32_t3(1.00f, 0.96f, 0.88f);
static const float32_t kCloudPhaseBase = 0.45f;        // 太陽の方を見たとき、雲が明るく光る(前方散乱)
static const float32_t kCloudPhaseForward = 2.4f;
static const float32_t kCloudPhasePower = 7.0f;
static const float32_t kCloudLightGain = 0.8f;
static const float32_t kCloudDisplayGamma = 2.2f;      // 雲の色は表示用の値で決めているので、リニアへ戻す
static const float32_t kFarDepth = 0.99999f;           // これ以上の深度は、何も描かれていない(無限遠)
static const float32_t kInfiniteDistance = 100000.0f;
static const float32_t kSceneFogDistanceScale = 3.0f;  // 物体までの距離を、この倍率の厚みとして雲を積算する(物体の周りにも雲があるように見せ、霞ませる)
static const float32_t kSkyDirectionY = 0.001f;        // 視線の y 成分がこれより小さいと、層との交点の計算を避ける

// レンズフレアとレンズダスト(層を抜けて太陽が見え始めるころ)
static const float32_t kFlareCenter = 0.70f;           // 進行のどこで強くなるか
static const float32_t kFlareWidth = 0.14f;
static const uint32_t kFlareGhosts = 5u;               // ゴースト(太陽と画面中心を結ぶ線上の、いくつもの丸)
static const float32_t kFlareGhostBase = -0.35f;       // 線上の位置(太陽と中心の間の割合)
static const float32_t kFlareGhostStep = -0.55f;
static const float32_t kFlareGhostRadiusBase = 0.05f;
static const float32_t kFlareGhostRadiusStep = 0.035f;
static const float32_t kFlareGhostGain = 0.55f;
static const float32_t kFlareGhostFalloff = 0.5f;
static const float32_t kFlareHaloRadius = 0.34f;
static const float32_t kFlareHaloWidth = 0.10f;
static const float32_t kFlareHaloGain = 0.5f;
static const float32_t kFlareGlowFalloff = 3.0f;
static const float32_t kFlareGlowGain = 0.7f;
static const float32_t kFlareFadeNear = 0.4f;          // 太陽が画面の中心から遠いほど弱める範囲
static const float32_t kFlareFadeFar = 1.6f;
static const float32_t3 kFlareColor = float32_t3(1.0f, 0.92f, 0.80f);
static const float32_t kDustBlobScaleA = 9.0f;         // ダスト: 柔らかい汚れ(2 種類の大きさ)と、細い繊維
static const float32_t kDustBlobScaleB = 17.0f;
static const float32_t kDustHairScale = 52.0f;
static const float32_t kDustGain = 1.1f;
static const float32_t kDustHairWeight = 0.12f;        // 細い繊維の強さ(強いと、上から降る縦線に見える)
static const float32_t kFlareBaseGain = 0.85f;

// 層を抜けた瞬間の光
static const float32_t kBurstCenter = 0.66f;           // 進行のどこで起こるか
static const float32_t kBurstWidth = 0.06f;
static const float32_t kBurstGain = 0.10f;
static const float32_t3 kBurstColor = float32_t3(1.0f, 0.94f, 0.80f);
static const float32_t kExposure = 1.25f;              // 全体の露出
static const float32_t kOutputGamma = 0.92f;

float32_t Fbm01(float32_t2 p) {
    return FbmN(p, kNoiseOctaves) / kNoiseOctaveNormalize;
}

// 画面の点から中心へ向かう線上を kZoomBlurTaps 回サンプルした平均(ズームブラー)。amount: 中心へ引き込む割合
float32_t3 ZoomBlurSample(float32_t2 uv, float32_t2 center, float32_t amount) {
    float32_t2 toCenter = center - uv;
    float32_t3 sum = float32_t3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (uint32_t i = 0; i < kZoomBlurTaps; ++i) {
        float32_t t = float32_t(i) / float32_t(kZoomBlurTaps - 1u);
        sum += gTexture.Sample(gSampler, uv + toCenter * amount * t).rgb;
    }
    return sum / float32_t(kZoomBlurTaps);
}

// 画面をサンプルする(画面の外は端に張り付ける)
float32_t3 SampleScene(float32_t2 uv) {
    return gTexture.Sample(gSampler, saturate(uv)).rgb;
}

// ---- 雲を抜ける(雲突入) ----

float32_t Hash31(int3 p) {
    // 3 次元の格子点の乱数。2 次元の整数ハッシュを、座標をずらして組み合わせる
    return Hash21(int2(p.x + p.z * 73, p.y + p.z * 131));
}

float32_t ValueNoise3(float32_t3 p) {
    int3 i = int3(floor(p));
    float32_t3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float32_t a = Hash31(i);
    float32_t b = Hash31(i + int3(1, 0, 0));
    float32_t c = Hash31(i + int3(0, 1, 0));
    float32_t d = Hash31(i + int3(1, 1, 0));
    float32_t e = Hash31(i + int3(0, 0, 1));
    float32_t g = Hash31(i + int3(1, 0, 1));
    float32_t h = Hash31(i + int3(0, 1, 1));
    float32_t k = Hash31(i + int3(1, 1, 1));
    return lerp(lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y), lerp(lerp(e, g, f.x), lerp(h, k, f.x), f.y), f.z);
}

float32_t Fbm3(float32_t3 p) {
    float32_t sum = 0.0f;
    float32_t amplitude = 0.5f;
    [unroll]
    for (uint32_t i = 0; i < 3u; ++i) {
        sum += amplitude * ValueNoise3(p);
        p = p * 2.02f + float32_t3(5.2f, 1.3f, 3.7f);
        amplitude *= 0.5f;
    }
    return sum / 0.875f; // 3 層の最大値(1 - 0.5^3)
}

// 雲の層の中の濃さ。boost: 遷移の進行に応じた、層全体の濃さの倍率
float32_t CloudDensity(float32_t3 p, float32_t boost) {
    float32_t height = (p.y - gSlabBottom) / (gSlabTop - gSlabBottom);
    float32_t profile = smoothstep(0.0f, kCloudProfileRiseEnd, height) * smoothstep(1.0f, kCloudProfileFallStart, height);
    float32_t n = Fbm3(p * kCloudNoiseScale);
    return max(0.0f, n - kCloudDensityThreshold) * kCloudDensityGain * profile * boost;
}

// 2 次元の値ノイズ(水の膜・ダスト用)。整数ハッシュ
float32_t Noise2(float32_t2 p) {
    return ValueNoise(p);
}

float32_t Fbm2(float32_t2 p) {
    return FbmN(p, kNoiseOctaves) / kNoiseOctaveNormalize;
}

// 画面の UV から、世界の視線の向きを求める
float32_t3 ViewRay(float32_t2 uv) {
    float32_t2 ndc = float32_t2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float32_t4 nearPos = mul(float32_t4(ndc, 0.0f, 1.0f), gInvViewProj);
    float32_t4 farPos = mul(float32_t4(ndc, 1.0f, 1.0f), gInvViewProj);
    return normalize(farPos.xyz / farPos.w - nearPos.xyz / nearPos.w);
}

float32_t3 CloudPassage(float32_t2 uv, float32_t2 pixel, float32_t time, float32_t duration, float32_t sceneDistance) {
    float32_t progress = saturate(time / max(duration, kMinLength));
    float32_t boostWave = (progress - kCloudBoostCenter) / kCloudBoostWidth;
    float32_t boost = kCloudBoostBase + kCloudBoostPeak * exp(-boostWave * boostWave);

    float32_t3 dir = ViewRay(uv);
    float32_t3 origin = gCameraPosition;

    // 視線が雲の層と交わる区間(層の外の遠くは、シーンの深度で打ち切る)
    float32_t invY = 1.0f / (abs(dir.y) < kSkyDirectionY ? kSkyDirectionY : dir.y);
    float32_t tA = (gSlabBottom - origin.y) * invY;
    float32_t tB = (gSlabTop - origin.y) * invY;
    float32_t t0 = max(min(tA, tB), 0.0f);
    float32_t t1 = max(tA, tB);
    if (origin.y > gSlabBottom && origin.y < gSlabTop) {
        t0 = 0.0f;
        t1 = abs(dir.y) < kSkyDirectionY ? kCloudMaxMarch : ((dir.y > 0.0f) ? tB : tA);
    }
    // 物体の手前だけでなく、周りの雲も見積もる(近い物体ほど手前の雲が薄く、背景だけが濃くなって影絵に見えるため)
    t1 = min(min(t1, t0 + kCloudMaxMarch), sceneDistance * kSceneFogDistanceScale);

    float32_t3 sunDir = normalize(gSunDirection);
    float32_t transmittance = 1.0f;
    float32_t3 cloudColor = float32_t3(0.0f, 0.0f, 0.0f);
    if (t1 > t0) {
        float32_t dt = (t1 - t0) / float32_t(kCloudSteps);
        float32_t t = t0 + dt * Hash21(int2(pixel));
        [loop]
        for (uint32_t i = 0; i < kCloudSteps; ++i) {
            float32_t3 p = origin + dir * t;
            float32_t d = CloudDensity(p, boost);
            if (d > kCloudMinDensity) {
                float32_t lightDensity = CloudDensity(p + sunDir * kCloudLightStep, boost);
                float32_t lit = exp(-lightDensity * kCloudLightFalloff);
                float32_t height = saturate((p.y - gSlabBottom) / (gSlabTop - gSlabBottom));
                float32_t3 ambient = lerp(kCloudAmbientLow, kCloudAmbientHigh, height);
                float32_t phase = kCloudPhaseBase + kCloudPhaseForward * pow(max(dot(dir, sunDir), 0.0f), kCloudPhasePower);
                float32_t3 color = ambient + kCloudSunLight * lit * phase * kCloudLightGain;
                float32_t alpha = 1.0f - exp(-d * dt * kCloudAbsorption);
                cloudColor += transmittance * alpha * color;
                transmittance *= 1.0f - alpha;
                if (transmittance < kCloudOpaqueCutoff) {
                    break;
                }
            }
            t += dt;
        }
    }

    // 画面の中心を原点とした座標(画面の高さを 1 とする)。レンズフレアとダストに使う
    float32_t2 centered = (uv - 0.5f) * float32_t2(gAspect, 1.0f);

    // 背景は、表示用の値(ガンマ空間)へ戻す(水滴は、2 つ目のパスが、この結果を屈折して描く)。
    // 背景(sceneColor)と、足し込む光(glow: 雲・ハイライト・フレア・光)は分けて持つ。
    // 露出(トーンマップ)は glow にだけ掛ける。背景にも掛けると、真っ白な背景が灰色に落ちて、
    // 光の加わった雲より暗い影絵になるため
    float32_t3 scene;
    scene = SampleScene(uv);
    scene = pow(scene, 1.0f / kCloudDisplayGamma);
    float32_t3 sceneColor = scene * transmittance;
    float32_t3 glow = cloudColor;

    // ---- レンズフレアとレンズダスト ----
    float32_t4 sunClip = mul(float32_t4(gCameraPosition + sunDir * kCloudMaxMarch, 1.0f), gViewProj);
    float32_t3 flareTotal = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t flareStrength = exp(-pow((progress - kFlareCenter) / kFlareWidth, 2.0f));
    if (sunClip.w > 0.0f) {
        float32_t2 sunNdc = sunClip.xy / sunClip.w;
        float32_t2 sunUv = float32_t2(sunNdc.x * 0.5f + 0.5f, 0.5f - sunNdc.y * 0.5f);
        float32_t2 toSun = (sunUv - 0.5f) * float32_t2(gAspect, 1.0f);
        float32_t3 ghosts = float32_t3(0.0f, 0.0f, 0.0f);
        [unroll]
        for (uint32_t g = 0; g < kFlareGhosts; ++g) {
            float32_t fg = float32_t(g);
            float32_t2 ghostPos = toSun * (kFlareGhostBase + kFlareGhostStep * fg);
            float32_t radius = kFlareGhostRadiusBase + kFlareGhostRadiusStep * fg;
            float32_t ring = smoothstep(radius, radius * 0.6f, length(centered - ghostPos)) * 0.6f;
            float32_t3 tint = 0.5f + 0.5f * cos(kTwoPi * (fg * 0.17f + float32_t3(0.0f, 0.33f, 0.67f)));
            ghosts += tint * ring * (kFlareGhostGain / (1.0f + fg * kFlareGhostFalloff));
        }
        float32_t halo = smoothstep(kFlareHaloWidth, 0.0f, abs(length(centered - toSun) - kFlareHaloRadius)) * kFlareHaloGain;
        float32_t glow = exp(-length(centered - toSun) * kFlareGlowFalloff) * kFlareGlowGain;
        float32_t inside = smoothstep(kFlareFadeFar, kFlareFadeNear, length(toSun));
        flareTotal = (ghosts + kFlareColor * (halo * 0.5f + glow)) * inside * flareStrength;
    }
    float32_t dustBlob = smoothstep(0.68f, 0.9f, Noise2(centered * kDustBlobScaleA + 3.0f)) * 0.9f
                       + smoothstep(0.72f, 0.92f, Noise2(centered * kDustBlobScaleB + 11.0f)) * 0.6f;
    float32_t dustHair = smoothstep(0.80f, 0.97f, Noise2(float32_t2(centered.x * kDustHairScale + centered.y * 5.0f, centered.y * 3.0f + 7.0f))) * kDustHairWeight;
    glow += flareTotal * (kFlareBaseGain + (dustBlob + dustHair) * kDustGain);

    // 層を抜けた瞬間の全体の光
    float32_t burstWave = (progress - kBurstCenter) / kBurstWidth;
    glow += kBurstColor * exp(-burstWave * burstWave) * kBurstGain;

    // 露出(トーンマップ)は光だけに掛け、背景に足す。光が無い画素は、背景がそのまま出る
    glow = 1.0f - exp(-glow * kExposure);
    glow = pow(saturate(glow), kOutputGamma);
    float32_t3 color = saturate(sceneColor + glow);
    return pow(color, kCloudDisplayGamma);
}

// ---- レンズの水滴 ----

// 1 層分の水滴。mask: 水滴の中(1)、trail: 流れた跡、lensUv: 水滴の中心の画面 UV、normal: 水滴の中の位置(-1〜1)
struct DropletResult {
    float32_t mask;
    float32_t trail;
    float32_t2 lensUv;
    float32_t2 normal;
};

DropletResult DropletLayer(float32_t2 uv, float32_t scale, int32_t seed, float32_t seedValue, float32_t cover, float32_t time) {
    DropletResult result;

    float32_t2 p = float32_t2(uv.x * gAspect, uv.y) * scale;
    // 水滴は下(uv の +y 方向)へ流れる。パターンの座標を逆方向へずらす
    float32_t flow = time * (kDropletFlowSpeedBase + kDropletFlowSpeedVariance * Hash21(int2(seed, 1)));
    p.y -= flow;
    int2 id = int2(floor(p));
    float32_t2 st = frac(p);

    float32_t n = Hash21(id + int2(seed, 0));
    float32_t n2 = Hash21(id + int2(seed + 31, 17));
    float32_t n3 = Hash21(id + int2(seed + 7, 53));

    float32_t appear = smoothstep(n * kDropletAppearSpread, n * kDropletAppearSpread + kDropletAppearLength, cover);
    float32_t2 center = float32_t2(0.5f + (n2 - 0.5f) * kDropletOffsetX, kDropletCenterYBase + kDropletCenterYVariance * n3);
    float32_t radius = (kDropletRadiusBase + kDropletRadiusVariance * n) * appear;

    float32_t2 q = st - center;
    q.y *= kDropletStretch;
    float32_t dist = length(q);
    result.mask = smoothstep(radius, radius * 0.82f, dist);
    result.normal = q / max(radius, kMinLength);

    // 跡: 水滴より上(流れてきた側)に、細く伸びる。先へ行くほど細く、薄くなる
    float32_t above = step(st.y, center.y);
    float32_t across = abs(st.x - center.x);
    float32_t width = radius * kDropletTrailWidth * lerp(kDropletTrailTipWidth, 1.0f, st.y / max(center.y, kMinLength));
    float32_t hasTrail = step(kDropletTrailChance, n2);
    result.trail = hasTrail * above * smoothstep(width, width * 0.3f, across) * smoothstep(0.0f, center.y * 0.9f, st.y) * appear;

    // 水滴の中心の画面 UV(パターンの座標から画面へ戻す)
    float32_t2 centerP = float32_t2(id) + center;
    centerP.y += flow;
    result.lensUv = float32_t2(centerP.x / gAspect, centerP.y) / scale;
    return result;
}

// 画面を、円盤状に散らしたサンプルでぼかす(くもったガラスの向こう)
float32_t3 BlurScene(float32_t2 uv, float32_t radius) {
    float32_t3 sum = float32_t3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (uint32_t i = 0; i < kDropletBlurTaps; ++i) {
        float32_t fi = float32_t(i);
        float32_t angle = fi * kGoldenAngle;
        float32_t rr = sqrt((fi + 0.5f) / float32_t(kDropletBlurTaps)) * radius;
        sum += SampleScene(uv + float32_t2(cos(angle) / gAspect, sin(angle)) * rr);
    }
    return sum / float32_t(kDropletBlurTaps);
}

float32_t3 LensDroplets(float32_t2 uv, float32_t cover, float32_t time) {
    float32_t blurRadius = kDropletBlurBase + cover * kDropletBlurGain;
    float32_t3 color = BlurScene(uv, blurRadius);
    color = lerp(color, color * kDropletTintColor, cover * kDropletTint);

    [unroll]
    for (uint32_t layer = 0; layer < kDropletLayers; ++layer) {
        float32_t fl = float32_t(layer);
        float32_t scale = lerp(kDropletScaleSmall, kDropletScaleLarge, fl / float32_t(max(kDropletLayers - 1u, 1u)));
        int32_t seed = kDropletSeedBase + kDropletSeedStep * int32_t(layer);
        DropletResult drop = DropletLayer(uv, scale, seed, float32_t(seed), cover, time);

        // 流れた跡: くもりが晴れて、背景が少しくっきり見える
        float32_t3 clear = BlurScene(uv, blurRadius * kDropletTrailClearBlur);
        color = lerp(color, clear, drop.trail * kDropletTrailStrength);

        // 水滴: 像が上下反転して見える(レンズ)。縁は暗く、上に小さなハイライト
        float32_t2 lensSampleUv = drop.lensUv + (drop.lensUv - uv) * kDropletLensMagnify;
        float32_t3 lens = SampleScene(lensSampleUv);
        float32_t rim = smoothstep(kDropletRimStart, 1.0f, length(drop.normal));
        lens *= 1.0f - kDropletRimDarken * rim;
        float32_t highlight = smoothstep(kDropletHighlightSize, 0.0f, length(drop.normal - kDropletHighlightPos));
        lens += highlight * kDropletHighlightGain;
        color = lerp(color, lens, drop.mask);
    }

    return lerp(color, gColor, smoothstep(kDropletWhiteStart, 1.0f, cover));
}

// ---- 霧 ----

// 画面の点の、視点から物体までの距離(深度の無い背景は、有限の遠距離にする)。isFar: 深度が無い画素か
float32_t MistDistance(float32_t2 uv, out bool isFar) {
    uint32_t width;
    uint32_t height;
    gDepth.GetDimensions(width, height);
    float32_t2 clamped = saturate(uv);
    int2 pixel = int2(clamped * float32_t2(width - 1u, height - 1u));
    float32_t depth = gDepth.Load(int3(pixel, 0));
    float32_t4 worldPos = mul(float32_t4(clamped.x * 2.0f - 1.0f, 1.0f - clamped.y * 2.0f, depth, 1.0f), gInvViewProj);
    isFar = depth >= kFarDepth;
    if (!isFar) {
        return min(length(worldPos.xyz / worldPos.w - gCameraPosition), gMistFog2.x);
    }
    // 深度の無い背景(空、雲海): 既定は一定の遠距離。水平な層(雲海、水面)があるときは、視線との交点までの距離にする
    float32_t distance = gMistFog2.x;
    if (gMistPlane.y > kMistPlaneEnabledThreshold) {
        float32_t3 dir = ViewRay(clamped);
        if (dir.y < -kSkyDirectionY) {
            float32_t t = (gMistPlane.x - gCameraPosition.y) / dir.y;
            if (t > 0.0f) {
                distance = min(t, distance);
            }
        }
    }
    return distance;
}

// 霧がかかった画面(水滴より奥。水滴の中でも、同じ霧の画面を屈折して見る)
float32_t3 MistShade(float32_t2 uv) {
    bool isFar;
    float32_t distance = MistDistance(uv, isFar);
    float32_t amount = MistAmount(distance, uv, gAspect, gTime, gCover, gMistFog.zw, gMistFog2.y) * gMistDrop.y;
    float32_t3 scene = SampleScene(uv);
    float32_t3 blurred = BlurScene(uv, gMistFog.y * amount);
    return ApplyMistFog(scene, blurred, amount, ViewRay(uv), normalize(gSunDirection), gMistFog.x, gMistFog2.z, gColor);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    float32_t3 scene = gTexture.Sample(gSampler, uv).rgb;
    float32_t3 result = scene;

    if (abs(gKind - kKindFade) < kKindTolerance) {
        // フェード: 単色へ一様に溶かす
        result = lerp(scene, gColor, gCover);
    } else if (abs(gKind - kKindNoiseWipe) < kKindTolerance) {
        // ノイズワイプ: ノイズの値が小さい所から先に覆う。縁のやわらかさ分だけ余分に進めて、覆い具合 1 で完全に覆う
        float32_t2 p = float32_t2(uv.x * gAspect, uv.y) * gNoiseScale;
        float32_t n = Fbm01(p);
        float32_t reach = gCover * (1.0f + gSoftness);
        float32_t mask = saturate((reach - n) / max(gSoftness, kMinSoftness));
        result = lerp(scene, gColor, mask);
    } else if (abs(gKind - kKindZoomBlur) < kKindTolerance) {
        // ズームブラー: 画面の点を、中心へ向かう線上の何点かの平均にする。覆い具合が高いほど長く引き込む
        float32_t3 blurred = ZoomBlurSample(uv, gCenter, gBlurStrength * gCover);
        float32_t toColor = smoothstep(kZoomBlurFadeStart, 1.0f, gCover);
        result = lerp(blurred, gColor, toColor);
    } else if (abs(gKind - kKindRipple) < kKindTolerance) {
        // 水の波紋: 中心から外へ広がる円形の波で画面を歪ませ、波の先頭より内側を単色で覆う
        float32_t2 d = (uv - gCenter) * float32_t2(gAspect, 1.0f);
        float32_t r = length(d);
        float32_t front = gCover * kRippleMaxRadius;
        float32_t wave = sin((r - front) * kRippleFrequency) * exp(-abs(r - front) * kRippleDecay);
        float32_t strength = gCover * (1.0f - gCover) * 4.0f; // 0 -> 1 -> 0。始まりと終わりでは歪めない
        float32_t2 offset = (d / max(r, kMinLength)) * wave * kRippleAmplitude * strength;
        offset.x /= gAspect;
        float32_t3 distorted = gTexture.Sample(gSampler, uv + offset).rgb;
        float32_t covered = 1.0f - smoothstep(front - kRippleEdgeWidth, front, r);
        result = lerp(distorted, gColor, covered);
    } else if (abs(gKind - kKindDroplets) < kKindTolerance) {
        result = LensDroplets(uv, gCover, gTime);
    } else if (abs(gKind - kKindCloudDive) < kKindTolerance) {
        // シーンの深度から、視点からその画素までの距離を求める(何も描かれていない所は無限遠)
        float32_t depth = gDepth.Load(int3(int2(input.position.xy), 0));
        float32_t4 worldPos = mul(float32_t4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f), gInvViewProj);
        float32_t sceneDistance = depth >= kFarDepth ? kInfiniteDistance : length(worldPos.xyz / worldPos.w - gCameraPosition);
        result = CloudPassage(uv, input.position.xy, gTime, gDuration, sceneDistance);
    } else if (abs(gKind - kKindMistLens) < kKindTolerance) {
        if (gMistFog2.w > kMistDebugViewThreshold) {
            // 距離の確認: 近いほど暗く、背景の距離で白。深度の無い画素は赤みをつける
            bool isFar;
            float32_t distance = MistDistance(uv, isFar);
            float32_t g = saturate(distance / gMistFog2.x);
            result = isFar ? float32_t3(g, g * kMistDebugFarTint, g * kMistDebugFarTint) : float32_t3(g, g, g);
        } else {
            result = MistShade(uv);
        }
    }

    output.color = float32_t4(result, 1.0f);
    return output;
}
