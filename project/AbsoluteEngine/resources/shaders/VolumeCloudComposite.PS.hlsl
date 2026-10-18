#include "Fullscreen.hlsli"
#include "VolumeCloudParam.hlsli"

// ボリューム雲のパス 3: 低解像度の雲を、元の解像度へ拡大して、シーンの色へ重ねる。
// 描画先(シーンの色)へプリマルチプライドでブレンドする: 結果 = 雲の光 + 元の画面 * (1 - alpha)
//
// 深度を見る拡大(バイラテラル・アップサンプル):
//   元の画素ごとに、まわりの低解像度の画素 4 つを混ぜる。ふつうの線形補間では、物体の縁で、
//   背景の画素に手前の物体の分の雲(や、雲なしの値)が混ざってにじむ。
//   そこで、4 つそれぞれの「物体までの距離」と、この画素の距離を比べ、近い距離の画素ほど重くする。

Texture2D<float32_t4> gCloudLow : register(t0);    // 低解像度の雲(rgb = 光、a = 1 - 透過率)
Texture2D<float32_t> gDistanceLow : register(t1);  // 低解像度の、物体までの距離(パス 1 の結果)
Texture2D<float32_t> gDepth : register(t2);        // シーンの深度(元の解像度)

static const float32_t kDistanceEpsilon = 0.01f;   // 距離の差(割合)の重みで、0 割りを避ける下駄(小さいほど、距離の一致を厳しく見る)
static const float32_t kMinDistance = 1.0f;        // 距離の差を割合にするときの、割る距離の下限

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // この画素の、物体までの距離
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);
    float32_t depth = gDepth.Load(int3(int2(input.position.xy), 0));
    float32_t distanceFull = DistanceFromDepth(ndc, depth);

    // この画素の中心が、低解像度の画素の座標(画素の中心が整数)でどこにあたるか。まわりの 4 画素と、その線形補間の重み
    float32_t2 lowPosition = input.position.xy / gDivisor - 0.5f;
    int2 base = int2(floor(lowPosition));
    float32_t2 fraction = lowPosition - float32_t2(base);
    int2 maxLow = int2(gLowSize) - 1;

    float32_t4 sum = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    float32_t weightSum = 0.0f;
    [unroll]
    for (int dy = 0; dy < 2; ++dy) {
        [unroll]
        for (int dx = 0; dx < 2; ++dx) {
            int2 coord = clamp(base + int2(dx, dy), int2(0, 0), maxLow);
            float32_t bilinear = (dx == 0 ? 1.0f - fraction.x : fraction.x) * (dy == 0 ? 1.0f - fraction.y : fraction.y);

            // 距離が近い画素ほど重くする(差は、この画素の距離に対する割合で見る)
            float32_t distanceLow = gDistanceLow.Load(int3(coord, 0));
            float32_t relativeDifference = abs(distanceLow - distanceFull) / max(distanceFull, kMinDistance);
            float32_t weight = bilinear / (kDistanceEpsilon + relativeDifference);

            sum += gCloudLow.Load(int3(coord, 0)) * weight;
            weightSum += weight;
        }
    }

    output.color = sum / max(weightSum, 1.0e-6f);
    return output;
}
