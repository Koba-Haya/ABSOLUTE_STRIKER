#include "Fullscreen.hlsli"
#include "VolumeCloudParam.hlsli"

// ボリューム雲のパス 1: シーンの深度を、低解像度へ縮める。
// 縮めた画素 1 つにつき、元の画素のブロック(gDivisor x gDivisor)のうち一番近い深度を選び、
// カメラから物体までの距離(ワールド)にして保存する(何も描かれていなければ無限遠)。
// 雲のレイマーチは、この距離で積算を打ち切る。拡大合成も、この距離と元の画素の距離を比べて、
// 物体の縁に雲の色がにじまないように重みを決める。

Texture2D<float32_t> gDepth : register(t0); // シーンの深度(元の解像度)

struct PixelShaderOutput {
    float32_t distance : SV_TARGET0; // R32_FLOAT
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // この低解像度の画素に対応する、元の画素のブロックの左上
    int2 block = int2(input.position.xy) * int(gDivisor);
    int2 maxPixel = int2(gFullSize) - 1;
    int divisor = int(gDivisor);

    float32_t nearest = 1.0f;
    [loop]
    for (int y = 0; y < divisor; ++y) {
        [loop]
        for (int x = 0; x < divisor; ++x) {
            int2 coord = min(block + int2(x, y), maxPixel); // 画面の端からはみ出さないようにする
            nearest = min(nearest, gDepth.Load(int3(coord, 0)));
        }
    }

    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);
    output.distance = DistanceFromDepth(ndc, nearest);
    return output;
}
