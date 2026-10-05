#ifndef SKY_HLSLI
#define SKY_HLSLI

// 夕焼けの空の色を返す。dir: 視線方向(正規化済み)、sunDir: 太陽の「方向」(太陽へ向かう向き、正規化済み)
// 水面の反射(Water.PS)からも呼ぶので、空と水面の色は常に一致する。
// 出力は RTV が sRGB なのでリニア値で書く。

static const float32_t3 kSkyZenithColor  = float32_t3(0.02f, 0.05f, 0.22f); // 天頂: 深い紺
static const float32_t3 kSkyMiddleColor  = float32_t3(0.45f, 0.14f, 0.28f); // 中間: 赤紫
static const float32_t3 kSkyHorizonColor = float32_t3(1.00f, 0.38f, 0.10f); // 地平線: 橙
static const float32_t3 kSunColor        = float32_t3(1.00f, 0.80f, 0.45f);

float32_t3 SkyColor(float32_t3 dir, float32_t3 sunDir) {
    // 高さ(0=地平線, 1=真上)。pow で地平線付近の橙を広く残す
    float32_t height = pow(saturate(dir.y), 0.5f);

    // 地平線→中間→天頂の 2 段階グラデーション
    float32_t3 lower = lerp(kSkyHorizonColor, kSkyMiddleColor, saturate(height * 2.0f));
    float32_t3 sky = lerp(lower, kSkyZenithColor, saturate(height * 2.0f - 1.0f));

    // 地平線より下は暗くする(L0 では水面が覆うが、見えても破綻しないように)
    sky = lerp(sky, kSkyHorizonColor * 0.4f, saturate(-dir.y * 4.0f));

    // 太陽: 視線と太陽方向の内積(=cos)で距離を測る。1 に近いほど太陽に近い
    float32_t cosSun = saturate(dot(dir, sunDir));
    float32_t glowWide = pow(cosSun, 8.0f);    // 空全体にじわっと広がる光
    float32_t glowNear = pow(cosSun, 96.0f);   // 太陽の周りの明るい光
    float32_t disc = smoothstep(0.9996f, 0.9998f, cosSun); // 円盤(角半径 約 0.02 rad)

    sky += kSunColor * (glowWide * 0.25f + glowNear * 0.8f);
    sky = lerp(sky, kSunColor * 2.6f, disc);
    return sky;
}

#endif
