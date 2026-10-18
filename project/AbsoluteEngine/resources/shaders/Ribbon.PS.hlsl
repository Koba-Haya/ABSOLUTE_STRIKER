#include "Ribbon.hlsli"

// 光り方の調整値。リボンは加算合成で描かれる(rgb * alpha が背景に足される)
static const float kFadePower = 1.5f;         // 尾のフェードの強さ。大きいほど尾が早く薄くなる
static const float kEdgePower = 1.5f;         // 幅方向の端の薄れ方。大きいほど端がぼける
static const float kCorePower = 4.0f;         // 芯の細さ。大きいほど芯が細くなる
static const float3 kCoreColor = float3(1.0f, 1.0f, 1.0f); // 芯の色(白)

float4 main(VertexShaderOutput input) : SV_TARGET {
    // 年齢の割合 u: 先端 0 から尾 1。先端が最も明るく、尾に向かって薄くなる
    float fade = pow(saturate(1.0f - input.texcoord.x), kFadePower);

    // 幅方向の中心からの近さ across: 中心 1、両端 0 (v = 0..1 を -1..1 に直して絶対値を取る)
    float across = 1.0f - abs(input.texcoord.y * 2.0f - 1.0f);

    // 端はなめらかに薄れさせる(輪郭が硬い線に見えないように)
    float edge = pow(across, kEdgePower);

    // 芯: 中心ほど白に近づける。外側はリボンの色
    float core = pow(across, kCorePower);
    float3 rgb = lerp(input.color.rgb, kCoreColor, core);

    return float4(rgb, input.color.a * fade * edge);
}
