#include "object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    // モデル本来の頂点位置(スキニング前)。スキニングは計算シェーダーが済ませており、position は動いた後の値なので、別に受け取る
    float3 bindPosition : TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));

    float4 worldPos4 = mul(input.position, gTransformationMatrix.World);
    output.worldPosition = worldPos4.xyz;
    output.localPosition = input.bindPosition;

    return output;
}