struct Well {
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Vertex {
    float4 position;
    float2 texcoord;
    float3 normal;
    float3 pad;
};

struct VertexInfluence {
    uint4 boneIDs;
    float4 weights;
};

struct SkinningInformation {
    uint numVertices;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    if (vertexIndex < gSkinningInformation.numVertices) {
        Vertex input = gInputVertices[vertexIndex];
        VertexInfluence influence = gInfluences[vertexIndex];

        // Skinning calculation
        float4x4 skinMatrix = 
            influence.weights.x * gMatrixPalette[influence.boneIDs.x].skeletonSpaceMatrix +
            influence.weights.y * gMatrixPalette[influence.boneIDs.y].skeletonSpaceMatrix +
            influence.weights.z * gMatrixPalette[influence.boneIDs.z].skeletonSpaceMatrix +
            influence.weights.w * gMatrixPalette[influence.boneIDs.w].skeletonSpaceMatrix;

        float3x3 skinNormalMatrix = 
            influence.weights.x * (float3x3)gMatrixPalette[influence.boneIDs.x].skeletonSpaceInverseTransposeMatrix +
            influence.weights.y * (float3x3)gMatrixPalette[influence.boneIDs.y].skeletonSpaceInverseTransposeMatrix +
            influence.weights.z * (float3x3)gMatrixPalette[influence.boneIDs.z].skeletonSpaceInverseTransposeMatrix +
            influence.weights.w * (float3x3)gMatrixPalette[influence.boneIDs.w].skeletonSpaceInverseTransposeMatrix;

        Vertex output;
        output.position = mul(input.position, skinMatrix);
        output.position.w = 1.0f;
        output.normal = normalize(mul(input.normal, skinNormalMatrix));
        output.texcoord = input.texcoord;
        // pad は、モデル本来の頂点位置(スキニング前)。そのまま描画側へ渡す
        output.pad = input.pad;

        gOutputVertices[vertexIndex] = output;
    }
}
