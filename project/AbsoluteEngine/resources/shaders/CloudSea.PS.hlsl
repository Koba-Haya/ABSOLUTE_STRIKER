#include "Fullscreen.hlsli"
#include "CloudSea.hlsli"

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // texcoord(0〜1, 左上原点) → NDC(-1〜1, 上が +y)
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);

    // 同じ画面位置の「近い点」と「遠い点」をワールド座標へ戻し、その差が視線方向(Sky.PS と同じ)
    float32_t4 nearPos = mul(float32_t4(ndc, 0.0f, 1.0f), gInvViewProj);
    float32_t4 farPos = mul(float32_t4(ndc, 1.0f, 1.0f), gInvViewProj);
    float32_t3 dir = normalize(farPos.xyz / farPos.w - nearPos.xyz / nearPos.w);

    // 色は表示用の値で計算しているので、RTV(sRGB)へ出す前にリニアへ戻す
    float32_t3 color = CloudSeaColor(dir, normalize(gSunDirection), input.position.xy);
    output.color = float32_t4(pow(saturate(color), kCloudSeaGamma), 1.0f);
    return output;
}
