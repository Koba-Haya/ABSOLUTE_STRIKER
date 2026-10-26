#include "Fullscreen.hlsli"

// Vignette パラメータ
cbuffer VignetteParam : register(b0) {
    float32_t gScale;
    float32_t gPowValue;
    float32_t2 gPad;
    // 周囲を寄せる色(rgb)と、効果の強さ(w。0 で効果なし、1 で最大)。黒・強さ 1 が従来の暗転
    float32_t4 gTint;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    
    // 周囲を0に、中心になるほど明るくなるように計算で調整
    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    // correctだけで計算すると中心の最大値が0.0625で暗すぎるのでScaleで調整
    float32_t vignette = correct.x * correct.y * gScale;
    
    // とりあえず指定乗数でそれっぽくする
    vignette = saturate(pow(vignette, gPowValue));
    
    // 中心(vignette = 1)は元の色、周囲(vignette = 0)は gTint の色。黒なら、従来の「色に乗算」と同じ結果になる。
    // 強さ gTint.w で、元の色との混ぜ具合を決める
    float32_t3 tinted = lerp(gTint.rgb, output.color.rgb, vignette);
    output.color.rgb = lerp(output.color.rgb, tinted, gTint.w);
    
    return output;
}
