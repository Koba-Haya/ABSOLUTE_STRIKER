#ifndef SKY_HLSLI
#define SKY_HLSLI

#include "Noise.hlsli"

// 昼の空の色(グラデーション・太陽・雲)を返す。dir: 視線方向(正規化済み)、sunDir: 太陽の「方向」(太陽へ向かう向き、正規化済み)
// 水面の反射(Water.PS)からも呼ぶので、空と水面の色は常に一致する。
// 出力は RTV が sRGB なのでリニア値で書く。

static const float32_t3 kSkyZenithColor  = float32_t3(0.03f, 0.16f, 0.62f); // 天頂: 濃い青
static const float32_t3 kSkyMiddleColor  = float32_t3(0.18f, 0.44f, 0.92f); // 中間: 明るい青
static const float32_t3 kSkyHorizonColor = float32_t3(0.62f, 0.80f, 1.00f); // 地平線: 白っぽい水色(霞)
static const float32_t3 kSunColor        = float32_t3(1.00f, 0.95f, 0.80f);

// 雲を除いた空(グラデーション + 太陽)。水面の霞の色など、雲に依らない色が欲しいときに使う
float32_t3 SkyBackground(float32_t3 dir, float32_t3 sunDir) {
    // 高さ(0=地平線, 1=真上)。pow で地平線付近の霞を広く残す
    float32_t height = pow(saturate(dir.y), 0.6f);

    // 地平線→中間→天頂の 2 段階グラデーション
    float32_t3 lower = lerp(kSkyHorizonColor, kSkyMiddleColor, saturate(height * 2.0f));
    float32_t3 sky = lerp(lower, kSkyZenithColor, saturate(height * 2.0f - 1.0f));

    // 地平線より下は少し暗くする(水面が覆うが、見えても破綻しないように)
    sky = lerp(sky, kSkyHorizonColor * 0.6f, saturate(-dir.y * 4.0f));

    // 太陽: 視線と太陽方向の内積(=cos)で距離を測る。1 に近いほど太陽に近い
    float32_t cosSun = saturate(dot(dir, sunDir));
    float32_t glowWide = pow(cosSun, 8.0f);    // 空全体にじわっと広がる光
    float32_t glowNear = pow(cosSun, 96.0f);   // 太陽の周りの明るい光
    float32_t disc = smoothstep(0.9996f, 0.9998f, cosSun); // 円盤(角半径 約 0.02 rad)

    sky += kSunColor * (glowWide * 0.12f + glowNear * 0.4f);
    sky = lerp(sky, kSunColor * 2.2f, disc);
    return sky;
}

// --- 雲(ノイズ平面雲・2 層) ---
// 空の高さ height に水平な平面を仮定し、視線との交点のワールド座標(xz)でノイズをサンプルして濃さを決める。
// 位置がワールド座標なので、自機が進むと後ろへ流れる。高さの違う 2 層を重ねる:
//   高い層 = 大きくやわらかい雲の塊(ゆっくり流れる)、低い層 = 小さな雲(速く流れる)
// 2 層の流れる速さの差(視差)で、前進している感じが出る。
struct CloudParams {
    float32_t height;        // 雲の平面の高さ(ワールド y)
    float32_t scale;         // ノイズの細かさ(1 / 粗い層の 1 マスの大きさ)
    float32_t2 wind;         // 風で流れる速さ(unit/s)
    float32_t coverage;      // この値より濃いノイズが雲になる(大きいほど雲が少ない)
    float32_t softness;      // 雲の縁のやわらかさ
    float32_t macroStrength; // 「雲が出やすい地域 / 出にくい地域」のばらつきの強さ(塊の大きさのばらつきになる)
    float32_t opacity;       // この層の最大の濃さ
};
static const CloudParams kCloudHigh = { 140.0f, 0.0035f, float32_t2(1.5f, 0.6f), 0.47f, 0.22f, 0.16f, 1.0f };
static const CloudParams kCloudLow  = {  55.0f, 0.0110f, float32_t2(3.2f, 1.3f), 0.55f, 0.18f, 0.10f, 0.9f };

static const float32_t kCloudSunStepCells = 0.08f; // 影を調べるために太陽側へずらす距離(ノイズのマス目の単位)
static const float32_t kCloudShadowGain = 3.0f;    // 影の強さ
static const float32_t kCloudBacklitDarken = 0.4f; // 太陽の方を見たとき、厚い雲の芯を暗くする割合(まぶしさに負けないように)
static const float32_t3 kCloudLitColor = float32_t3(1.00f, 1.00f, 1.00f);
static const float32_t3 kCloudShadowColor = float32_t3(0.35f, 0.45f, 0.65f); // 日陰の雲(空の青をまとった灰色)
static const float32_t kCloudHorizonFadeStart = 0.02f; // 地平線に近い所は雲が細かく詰まって見えるので、薄くして霞に溶かす
static const float32_t kCloudHorizonFadeEnd = 0.30f;

// 雲の濃さ(0〜1)。fbm ノイズを、しきい値(coverage)から smoothstep で立ち上げる。
// しきい値自体を低周波ノイズでずらすので、雲が多い地域(大きな塊)と少ない地域(小さな雲が散らばる)ができる
float32_t CloudDensity(float32_t2 p, CloudParams cp) {
    float32_t2 q = p * cp.scale;
    float32_t n = FbmN(q, 5u) / 0.96875f; // 5 層の最大値(1 - 0.5^5)で割って 0〜1 に正規化
    float32_t macro = ValueNoise(q * 0.3f + 17.0f) * 2.0f - 1.0f; // -1〜1 の低周波
    float32_t coverage = cp.coverage - cp.macroStrength * macro;
    return smoothstep(coverage, coverage + cp.softness, n);
}

// 1 層分。戻り値: rgb = 雲の色、a = 雲の濃さ(0 なら雲なし)。origin: 視線の出発点(ワールド座標)、time: 風で流す秒
float32_t4 CloudLayer(float32_t3 dir, float32_t3 sunDir, float32_t3 origin, float32_t time, CloudParams cp) {
    if (dir.y <= kCloudHorizonFadeStart || origin.y >= cp.height) {
        return float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    // 視線と雲の平面の交点
    float32_t t = (cp.height - origin.y) / dir.y;
    float32_t2 p = origin.xz + dir.xz * t + cp.wind * time;
    float32_t density = CloudDensity(p, cp);

    // 陰影: 太陽の方向へ少しずらした点の方が濃ければ、太陽の光がその分さえぎられている(日陰)
    float32_t2 toSun = sunDir.xz / max(length(sunDir.xz), 1e-4f);
    float32_t densityTowardSun = CloudDensity(p + toSun * (kCloudSunStepCells / cp.scale), cp);
    float32_t lit = 1.0f - saturate((densityTowardSun - density) * kCloudShadowGain);
    lit *= 1.0f - 0.35f * density; // 厚い雲の中心ほど暗い
    float32_t3 color = lerp(kCloudShadowColor, kCloudLitColor, lit);

    // 逆光: 太陽の方を見ているときは、厚い雲の芯が暗くなり、薄い縁だけが明るく光る(シルバーライニング)
    float32_t cosSun = saturate(dot(dir, sunDir));
    color *= 1.0f - kCloudBacklitDarken * pow(cosSun, 4.0f) * density;
    float32_t edge = density * (1.0f - density) * 4.0f; // 縁(0 < 濃さ < 1)で 1 に近い
    color += kSunColor * pow(cosSun, 6.0f) * edge * 1.2f;

    float32_t fade = smoothstep(kCloudHorizonFadeStart, kCloudHorizonFadeEnd, dir.y);
    return float32_t4(color, density * fade * cp.opacity);
}

// 雲つきの空の色。origin / time は雲の計算に使う(水面の反射からは、反射の出発点 = 水面上の点を渡す)
// 奥(高い層)から手前(低い層)の順に重ねる
float32_t3 SkyColor(float32_t3 dir, float32_t3 sunDir, float32_t3 origin, float32_t time) {
    float32_t3 sky = SkyBackground(dir, sunDir);
    float32_t4 high = CloudLayer(dir, sunDir, origin, time, kCloudHigh);
    sky = lerp(sky, high.rgb, high.a);
    float32_t4 low = CloudLayer(dir, sunDir, origin, time, kCloudLow);
    return lerp(sky, low.rgb, low.a);
}

#endif
