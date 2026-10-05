#ifndef WATER_HLSLI
#define WATER_HLSLI

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

// 水面の法線。高さ h(x,z,t) = Σ A sin(k·p + ω t) の勾配から作る(解析的)。
// distance: カメラから水面上の点までの距離。遠いほど法線の揺れを弱め、ちらつき(エイリアシング)を抑える
float32_t3 WaterNormal(float32_t2 xz, float32_t distance, float32_t time) {
    float32_t2 gradient = float32_t2(0.0f, 0.0f);
    for (uint32_t i = 0; i < kWaveCount; ++i) {
        Wave w = kWaves[i];
        float32_t phase = dot(w.direction, xz) * w.frequency + time * w.speed;
        // d/dp [A sin(k d·p + ωt)] = A k cos(...) d
        gradient += w.direction * (w.amplitude * w.frequency * cos(phase));
    }
    float32_t fade = rcp(1.0f + distance * 0.04f);
    gradient *= fade;
    return normalize(float32_t3(-gradient.x, 1.0f, -gradient.y));
}

#endif
