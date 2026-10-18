#include "Ribbon.hlsli"

// リボンの頂点はワールド座標で渡されるので、ビュー射影行列だけを掛ける
cbuffer TransformationMatrix : register(b0) {
    float4x4 WVP;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(float4(input.position, 1.0f), WVP);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}
