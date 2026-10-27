#include "GPUParticle.hlsli"

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

ConstantBuffer<PerView> gPerView : register(b0);
StructuredBuffer<GPUParticle> gParticles : register(t0);

struct VertexShaderInput {
    float32_t3 position : POSITION;
    float32_t2 texcoord : TEXCOORD;
};

struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD;
    float32_t4 color : COLOR;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    GPUParticle particle = gParticles[instanceId];

    // 発光パス専用の粒(核の光など)は、通常の描画では描かない(大きさ 0 で潰す)
    if (particle.glowOnly > 0.5f) {
        particle.scale = float32_t3(0.0f, 0.0f, 0.0f);
    }

    // ビルボード行列とスケール・座標を組み合わせてWorld行列を作成
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    output.position = mul(float32_t4(input.position, 1.0f), mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    return output;
}
