#include "Fullscreen.hlsli"
#include "Sky.hlsli"

// b0: 空のパラメータ。行列は C++ と同じ行優先で受け取る
cbuffer SkyParam : register(b0) {
    row_major float4x4 gInvViewProj;
    float32_t3 gSunDirection;
    float32_t gTime;
    float32_t3 gCameraPosition;
    float32_t pad;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // texcoord(0〜1, 左上原点) → NDC(-1〜1, 上が +y)
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);

    // 同じ画面位置の「近い点」と「遠い点」をワールド座標へ戻し、その差が視線方向
    float32_t4 nearPos = mul(float32_t4(ndc, 0.0f, 1.0f), gInvViewProj);
    float32_t4 farPos = mul(float32_t4(ndc, 1.0f, 1.0f), gInvViewProj);
    float32_t3 dir = normalize(farPos.xyz / farPos.w - nearPos.xyz / nearPos.w);

    output.color = float32_t4(SkyColor(dir, normalize(gSunDirection), gCameraPosition, gTime), 1.0f);
    return output;
}
