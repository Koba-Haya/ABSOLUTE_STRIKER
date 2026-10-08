#ifndef WATER_HLSLI
#define WATER_HLSLI

#include "Noise.hlsli"

// 水面の高さ(ワールド y)
static const float32_t kWaterHeight = 0.0f;

// 波 1 本分: xz 平面での進行方向(正規化済み)、空間周波数(rad/unit)、振幅、速さ
struct Wave {
    float32_t2 direction;
    float32_t frequency;
    float32_t amplitude;
    float32_t speed;
};

// 大きなうねり → 細かいさざ波の順に 4 本重ねる
static const uint32_t kWaveCount = 4;
static const Wave kWaves[kWaveCount] = {
    { float32_t2(0.913f, 0.406f),   0.35f, 0.50f, 0.8f },
    { float32_t2(-0.287f, 0.958f),  0.80f, 0.25f, 1.3f },
    { float32_t2(0.600f, -0.800f),  1.90f, 0.12f, 2.1f },
    { float32_t2(-0.976f, -0.217f), 4.00f, 0.06f, 3.0f },
};

// 規則的な波(sin の和)にムラを入れる強さ
static const float32_t kWarpScale = 0.07f;          // 位置を歪めるノイズの粗さ(小さいほど大きなうねり)
static const float32_t kWarpStrength = 6.0f;        // 位置のずらし量
static const float32_t kAmplitudeNoiseScale = 0.03f; // 振幅ムラのノイズの粗さ
static const float32_t kAmplitudeMin = 0.45f;       // 穏やかな所の波の強さ
static const float32_t kAmplitudeMax = 1.4f;        // 荒れた所の波の強さ
static const float32_t kRippleScale = 0.9f;         // 細かいさざ波の細かさ
static const float32_t kRippleStrength = 0.15f;     // さざ波の強さ
static const float32_t kNoiseScrollSpeed = 0.05f;   // ノイズがゆっくり流れる速さ

// 水面の法線。高さ h(x,z,t) = Σ A sin(k·p + ω t) の勾配から作る(解析的)。
// そのままだと全面が同じ縞になるので、次の 3 つのノイズでムラを入れる:
//   (1) sin に入れる位置を、低周波ノイズでずらす(縞がうねる)
//   (2) 場所ごとに波の強さを変える(穏やかな所と荒れた所)
//   (3) 細かいさざ波(fbm の勾配)を足す
// distance: カメラから水面上の点までの距離。遠いほど法線の揺れを弱め、ちらつき(エイリアシング)を抑える
float32_t3 WaterNormal(float32_t2 xz, float32_t distance, float32_t time) {
    // (1) 位置の歪み
    float32_t2 drift = float32_t2(time * kNoiseScrollSpeed, time * kNoiseScrollSpeed * 0.7f);
    float32_t2 warp = float32_t2(ValueNoise(xz * kWarpScale + drift),
                                 ValueNoise(xz * kWarpScale + float32_t2(7.3f, 3.1f) - drift)) - 0.5f;
    float32_t2 p = xz + warp * kWarpStrength;

    // (2) 振幅のムラ
    float32_t amplitudeScale = lerp(kAmplitudeMin, kAmplitudeMax, ValueNoise(xz * kAmplitudeNoiseScale + drift * 0.4f));

    float32_t2 gradient = float32_t2(0.0f, 0.0f);
    for (uint32_t i = 0; i < kWaveCount; ++i) {
        Wave w = kWaves[i];
        float32_t phase = dot(w.direction, p) * w.frequency + time * w.speed;
        // d/dp [A sin(k d·p + ωt)] = A k cos(...) d
        gradient += w.direction * (w.amplitude * w.frequency * cos(phase));
    }
    gradient *= amplitudeScale;

    // (3) さざ波: 高さ = fbm の値とみなし、少しずらした点との差(有限差分)で勾配を取る
    const float32_t e = 0.1f;
    float32_t2 q = xz * kRippleScale + time * float32_t2(0.4f, 0.25f);
    float32_t h = Fbm(q);
    float32_t2 rippleGradient = float32_t2(Fbm(q + float32_t2(e, 0.0f)) - h, Fbm(q + float32_t2(0.0f, e)) - h) / e;
    gradient += rippleGradient * (kRippleStrength * kRippleScale);

    float32_t fade = rcp(1.0f + distance * 0.04f);
    gradient *= fade;
    return normalize(float32_t3(-gradient.x, 1.0f, -gradient.y));
}

#endif
