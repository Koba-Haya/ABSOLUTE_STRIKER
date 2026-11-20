#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct WellForGPU {
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<WellForGPU> gSkin : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    uint4 boneIDs : BONEIDS;
    float4 weights : WEIGHTS;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // Skinning calculation
    float4x4 skinMatrix = 0;
    float4x4 skinNormalMatrix = 0;

    [unroll]
    for (int i = 0; i < 4; ++i) {
        skinMatrix += input.weights[i] * gSkin[input.boneIDs[i]].skeletonSpaceMatrix;
        skinNormalMatrix += input.weights[i] * gSkin[input.boneIDs[i]].skeletonSpaceInverseTransposeMatrix;
    }

    float4 skinnedPosition = mul(input.position, skinMatrix);
    skinnedPosition.w = 1.0f;
    float3 skinnedNormal = normalize(mul(input.normal, (float3x3)skinNormalMatrix));

    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    output.normal = normalize(mul(skinnedNormal, (float3x3) gTransformationMatrix.WorldInverseTranspose));

    float4 worldPos4 = mul(skinnedPosition, gTransformationMatrix.World);
    output.worldPosition = worldPos4.xyz;
    // スキニング前の位置(バインドポーズ)。動いても、表面の同じ点は、同じ値になる
    output.localPosition = input.position.xyz;

    return output;
}
