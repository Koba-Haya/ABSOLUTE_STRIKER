// エネルギーのリボン(EnergyRibbon)のピクセルシェーダー。帯の上の座標から、光の帯の見た目を計算して描く。
//
// 帯の座標: texcoord.x = 年齢の割合(先端 0 から尾 1)、texcoord.y = 幅方向(左端 0 から右端 1)。
// 見た目(下の層から、上の層へ、アルファ合成で重ねる):
//   外側の暗い輪郭(明るい空の上でも、帯の形が見える)-> 明るい紫の光(内部に、尾から先端へ流れるノイズの模様)-> 細い白い芯
// 先端が最も明るく、尾へ向かって薄れる。頂点カラーのアルファは、帯全体の濃さに掛ける(色は、パラメータから取る)。
// 出力は、通常のアルファ合成。

#include "Ribbon.hlsli"

// EnergyRibbon.h の ParamsCB と並びを合わせること
struct EnergyRibbonParams {
    float4 glowColor;
    float4 coreColor;
    float4 outlineColor;
    float time;
    float seed;
    float intensity;
    float flowSpeed;
};
ConstantBuffer<EnergyRibbonParams> gParams : register(b0);

// 尾のフェードの強さ。大きいほど尾が早く薄くなる
static const float kFadePower = 1.3f;
// 先端が、急に切れないよう、先端の近く(年齢の割合 0 〜 この値)で、薄くしていく範囲
static const float kHeadFadeRange = 0.18f;
// 帯の幅方向の光の広がり(小さいほど、中心に集まる)と、芯の細さ(大きいほど細い)
static const float kBandPower = 1.0f;
static const float kCorePower = 8.0f;
// 暗い輪郭の濃さと、輪郭が立ち上がる幅(幅方向の中心からの近さ 0 〜 この値)
static const float kOutlineAlpha = 0.80f;
static const float kOutlineRise = 0.40f;
// 流れる模様の細かさ(帯の長さ方向、幅方向)と、明るさへの効き(下限と振れ幅)
static const float kFlowLengthFrequency = 5.0f;
static const float kFlowWidthFrequency = 2.5f;
static const float kFlowBase = 0.55f;
static const float kFlowAmount = 0.90f;
// 光の濃さの倍率
static const float kBandAlpha = 1.4f;

float Hash21(float2 p) {
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float ValueNoise(float2 p) {
    const float2 i = floor(p);
    const float2 f = frac(p);
    const float2 u = f * f * (3.0f - 2.0f * f);
    const float a = Hash21(i);
    const float b = Hash21(i + float2(1.0f, 0.0f));
    const float c = Hash21(i + float2(0.0f, 1.0f));
    const float d = Hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm(float2 p) {
    float value = 0.0f;
    float amplitude = 0.5f;
    for (int i = 0; i < 3; ++i) {
        value += amplitude * ValueNoise(p);
        p *= 2.03f;
        amplitude *= 0.5f;
    }
    return value;
}

// 下の層(bottom)の上に、上の層(top)を重ねる(アルファ合成。色はアルファで割った値で持つ)
float4 Over(float4 top, float4 bottom) {
    const float a = top.a + bottom.a * (1.0f - top.a);
    const float3 c = (top.rgb * top.a + bottom.rgb * bottom.a * (1.0f - top.a)) / max(a, 0.0001f);
    return float4(c, a);
}

float4 main(VertexShaderOutput input) : SV_TARGET {
    const float u = input.texcoord.x;
    const float v = input.texcoord.y;

    // 先端から尾へ、薄れていく。先端そのものも、なめらかに薄くする(帯の先が、棒の端のように見えないように)
    const float fade = pow(saturate(1.0f - u), kFadePower) * smoothstep(0.0f, kHeadFadeRange, u);
    // 幅方向の中心からの近さ: 中心 1、両端 0
    const float across = 1.0f - abs(v * 2.0f - 1.0f);

    // 流れる模様: 尾から先端へ向かって流れる。模様の位置は「u x 周波数 + 時間 x 速さ」が一定の所なので、時間が進むと、
    // u の小さいほう(先端のほう)へ動く
    const float flow = Fbm(float2(u * kFlowLengthFrequency + gParams.time * gParams.flowSpeed + gParams.seed * 7.0f,
                                  v * kFlowWidthFrequency + gParams.seed * 3.0f));

    float4 col = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 1. 暗い輪郭(帯の全幅。端はなめらかに消える)
    const float outlineAlpha = smoothstep(0.0f, kOutlineRise, across) * kOutlineAlpha * fade * input.color.a;
    col = Over(float4(gParams.outlineColor.rgb, outlineAlpha), col);

    // 2. 明るい紫の光(中心に近いほど濃い。流れる模様で、明るさが変わる)
    const float band = pow(across, kBandPower) * (kFlowBase + kFlowAmount * flow);
    const float bandAlpha = saturate(band * kBandAlpha) * fade * input.color.a;
    col = Over(float4(gParams.glowColor.rgb * gParams.intensity, bandAlpha), col);

    // 3. 細い白い芯
    const float coreAlpha = pow(across, kCorePower) * (kFlowBase + kFlowAmount * flow) * fade * input.color.a;
    col = Over(float4(gParams.coreColor.rgb * gParams.intensity, saturate(coreAlpha)), col);

    return float4(saturate(col.rgb), col.a);
}
