#include "Fullscreen.hlsli"
#include "VolumeCloudParam.hlsli"

// ボリューム雲のパス 3: 低解像度の雲を、元の解像度へ拡大して、シーンの色へ重ねる。
// 描画先(シーンの色)へプリマルチプライドでブレンドする: 結果 = 雲の光 + 元の画面 * (1 - alpha)
//
// 深度を見る拡大(バイラテラル・アップサンプル):
//   元の画素ごとに、まわりの低解像度の画素 3x3 を混ぜる。重みは 2 つの積。
//     空間の重み: 画素の中心に近いほど重い(ガウス関数)。幅を持たせて、ディザの粒(レイの開始位置のずらしによる、細かいドット)を
//                 ぼかして消す。
//     距離の重み: 4 つそれぞれの「物体までの距離」と、この画素の距離を比べ、近い距離の画素ほど重い。
//                 ふつうのぼかしでは、物体の縁で、背景の画素に手前の物体の分の雲(や、雲なしの値)が混ざってにじむ。
//                 距離で重みを付けると、物体の縁を越えて混ざらない。

Texture2D<float32_t4> gCloudLow : register(t0);    // 低解像度の雲(rgb = 光、a = 1 - 透過率)
Texture2D<float32_t> gDistanceLow : register(t1);  // 低解像度の、物体までの距離(パス 1 の結果)
Texture2D<float32_t> gDepth : register(t2);        // シーンの深度(元の解像度)

static const float32_t kDistanceEpsilon = 0.01f;   // 距離の差(割合)の重みで、0 割りを避ける下駄(小さいほど、距離の一致を厳しく見る)
static const float32_t kMinDistance = 1.0f;        // 距離の差を割合にするときの、割る距離の下限
static const float32_t kBlurSigmaLayer = 0.85f;    // 空間の重みの広がり(低解像度の画素単位)。層(雲海)は、ドットが目立つので広め
static const float32_t kBlurSigmaBox = 0.55f;      // 箱の雲は、輪郭を保つため狭め
static const float32_t kNoBlurSigma = 0.0001f;     // 等倍(分母 1)のときは、ぼかさない(中心の 1 画素だけが重みを持つ)
static const float32_t kShapeModeLayerThreshold = 0.5f; // gShapeMode はこの値より大きければ層
static const float32_t kMinWeightSum = 1.0e-6f;

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // この画素の、物体までの距離
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);
    float32_t depth = gDepth.Load(int3(int2(input.position.xy), 0));
    float32_t distanceFull = DistanceFromDepth(ndc, depth);

    // この画素の中心が、低解像度の画素の座標(画素の中心が整数)でどこにあたるか。一番近い低解像度の画素を中心に、3x3 を見る
    float32_t2 lowPosition = input.position.xy / gDivisor - 0.5f;
    int2 center = int2(floor(lowPosition + 0.5f));
    int2 maxLow = int2(gLowSize) - 1;

    float32_t sigma = gDivisor <= 1.0f ? kNoBlurSigma : (gShapeMode > kShapeModeLayerThreshold ? kBlurSigmaLayer : kBlurSigmaBox);
    float32_t inverseTwoSigmaSq = 1.0f / (2.0f * sigma * sigma);

    float32_t4 sum = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    float32_t weightSum = 0.0f;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy) {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx) {
            int2 tap = center + int2(dx, dy);
            int2 coord = clamp(tap, int2(0, 0), maxLow); // 画面の端からはみ出さないようにする

            // 空間の重み: この画素の中心(lowPosition)から、その低解像度の画素の中心までの距離
            float32_t2 offset = float32_t2(tap) - lowPosition;
            float32_t spatial = exp(-dot(offset, offset) * inverseTwoSigmaSq);

            // 距離の重み: 距離が近い画素ほど重い(差は、この画素の距離に対する割合で見る)
            float32_t distanceLow = gDistanceLow.Load(int3(coord, 0));
            float32_t relativeDifference = abs(distanceLow - distanceFull) / max(distanceFull, kMinDistance);
            float32_t weight = spatial / (kDistanceEpsilon + relativeDifference);

            sum += gCloudLow.Load(int3(coord, 0)) * weight;
            weightSum += weight;
        }
    }

    output.color = sum / max(weightSum, kMinWeightSum);
    return output;
}
