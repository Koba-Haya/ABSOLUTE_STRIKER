#include "Fullscreen.hlsli"

// 新方式の雲海の背景(空)。全画面パスで、雲の下・後ろの色だけを描く(雲そのものは、ボリューム雲の層が担う)。
//   上向きの視線: 地平線から天頂へのグラデーション
//   下向きの視線: 雲の切れ間から見える「下の層」。旧方式は、ここに雲のようなノイズを使っていたが、遠くを斜めに見ると
//                 ノイズが横に引き伸ばされて水平な縞になった。そこで、模様のない、なめらかな色にしている
//   太陽: 光の輪と、太陽の円盤
// 色は、旧方式の雲海(CloudSea.hlsli)と同じ表示用の値で書いて、最後にリニアへ戻す(描画先が sRGB のため)。

cbuffer SeaSkyParam : register(b0) {
    row_major float4x4 gInvViewProj;
    float32_t3 gCameraPosition;
    float32_t gFloorHeight;   // 切れ間から見える下の層の高さ(ワールド y)
    float32_t3 gSunDirection; // 太陽へ向かう向き(正規化済み)
    float32_t gGapDarkness;   // 下の層の暗さ(0〜1)
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t kGamma = 2.2f;                   // 表示用の値 -> リニア
// 8bit の色は 1/255 刻みなので、なめらかなグラデーションに段差(帯)が出る。
// 出力の直前に、1 段分(1/255)の大きさのごく小さなノイズを足して、段差を目立たなくする(ディザ)。
// 振幅が 1 段分なので、粒としては見えない
static const float32_t kDitherAmplitude = 1.0f / 255.0f;
static const float32_t2 kDitherScale = float32_t2(0.06711056f, 0.00583715f);  // 画素ごとの乱数(Interleaved Gradient Noise)
static const float32_t kDitherMul = 52.9829189f;
static const float32_t kSkyGradientPower = 0.5f;        // 地平線から天頂へのグラデーションの曲がり方
static const float32_t kMinLookDown = 0.002f;           // 視線の下向き成分がこれより小さい(ほぼ水平)なら、下の層までの距離を求めない
static const float32_t kFloorFog = 0.004f;              // 下の層を地平線の色へ溶かす強さ(距離に対して)
static const float32_t kFloorLightMix = 0.30f;          // 下の層の色に、明るい色を混ぜる割合(模様の代わりに、淡い明るさを足す)
static const float32_t kSunGlowPower = 48.0f;           // 太陽の光の輪(強い)の鋭さ
static const float32_t kSunGlowGain = 0.5f;
static const float32_t kSunDiskStart = 0.9990f;         // 太陽の円盤(cos がこの範囲で、0 -> 1)
static const float32_t kSunDiskEnd = 0.9996f;
static const float32_t kSunHaloPower = 8.0f;            // 太陽の光の輪(広い)の鋭さ
static const float32_t kSunHaloGain = 0.10f;

static const float32_t3 kHorizonColor = float32_t3(0.80f, 0.88f, 1.00f);
static const float32_t3 kZenithColor = float32_t3(0.12f, 0.35f, 0.80f);
static const float32_t3 kLowColor = float32_t3(0.32f, 0.46f, 0.74f);     // 下の層: 明るい側(暗さ 0)
static const float32_t3 kGapDarkColor = float32_t3(0.20f, 0.28f, 0.50f); // 下の層: 暗い側(暗さ 1)
static const float32_t3 kFloorLight = float32_t3(0.74f, 0.80f, 0.92f);
static const float32_t3 kSunColor = float32_t3(1.00f, 0.92f, 0.75f);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // texcoord(0〜1, 左上原点) -> NDC(-1〜1, 上が +y)。同じ画面位置の「近い点」と「遠い点」をワールドへ戻した差が視線方向
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);
    float32_t4 nearPos = mul(float32_t4(ndc, 0.0f, 1.0f), gInvViewProj);
    float32_t4 farPos = mul(float32_t4(ndc, 1.0f, 1.0f), gInvViewProj);
    float32_t3 dir = normalize(farPos.xyz / farPos.w - nearPos.xyz / nearPos.w);

    float32_t3 color;
    if (dir.y >= 0.0f) {
        color = lerp(kHorizonColor, kZenithColor, pow(saturate(dir.y), kSkyGradientPower));
    } else {
        // 下の層までの距離(視線が、下の層の高さへ届くまで)。遠いほど、地平線の色へ溶ける
        float32_t t = (gFloorHeight - gCameraPosition.y) / min(dir.y, -kMinLookDown);
        float32_t3 gap = lerp(kLowColor, kGapDarkColor, gGapDarkness);
        float32_t3 floorColor = lerp(gap, kFloorLight, kFloorLightMix);
        color = lerp(floorColor, kHorizonColor, 1.0f - exp(-t * kFloorFog));
    }

    float32_t cosSun = saturate(dot(dir, normalize(gSunDirection)));
    color += kSunColor * (pow(cosSun, kSunGlowPower) * kSunGlowGain + smoothstep(kSunDiskStart, kSunDiskEnd, cosSun) +
                          pow(cosSun, kSunHaloPower) * kSunHaloGain);

    // ディザ(-0.5〜0.5 の乱数 * 1 段分)を、表示用の値へ足してから、リニアへ戻す
    float32_t dither = frac(kDitherMul * frac(dot(input.position.xy, kDitherScale))) - 0.5f;
    color += dither * kDitherAmplitude;

    output.color = float32_t4(pow(saturate(color), kGamma), 1.0f);
    return output;
}
