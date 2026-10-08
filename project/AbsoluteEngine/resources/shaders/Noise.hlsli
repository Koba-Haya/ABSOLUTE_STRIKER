#ifndef NOISE_HLSLI
#define NOISE_HLSLI

// --- ノイズ(画像を使わず、整数ハッシュで作る値ノイズ) ---
// 水面の波のムラと、空の雲で共有する。

// 長時間表示しても(ワールド座標が大きくなっても)格子点のハッシュが崩れないよう、浮動小数の frac ではなく整数で混ぜる
float32_t Hash21(int2 p) {
    uint32_t h = uint32_t(p.x) * 374761393u + uint32_t(p.y) * 668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return float32_t(h) * (1.0f / 4294967295.0f);
}

// 格子点の乱数を、なめらかな補間(smoothstep 形)で結ぶ。出力は 0〜1
float32_t ValueNoise(float32_t2 p) {
    int2 cell = int2(floor(p));
    float32_t2 f = frac(p);
    float32_t2 u = f * f * (3.0f - 2.0f * f);
    float32_t a = Hash21(cell);
    float32_t b = Hash21(cell + int2(1, 0));
    float32_t c = Hash21(cell + int2(0, 1));
    float32_t d = Hash21(cell + int2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// 周波数を 2 倍ずつ、振幅を半分ずつにして octaves 層重ねる(fbm)。細かい揺れほど弱い。
// 出力は 0〜(1 - 0.5^octaves)。octaves は定数で呼ぶこと(ループが展開される)
float32_t FbmN(float32_t2 p, uint32_t octaves) {
    float32_t sum = 0.0f;
    float32_t amplitude = 0.5f;
    for (uint32_t i = 0; i < octaves; ++i) {
        sum += amplitude * ValueNoise(p);
        p *= 2.03f;
        amplitude *= 0.5f;
    }
    return sum;
}

// 水面のさざ波用の 3 層
float32_t Fbm(float32_t2 p) {
    return FbmN(p, 3u);
}

#endif
