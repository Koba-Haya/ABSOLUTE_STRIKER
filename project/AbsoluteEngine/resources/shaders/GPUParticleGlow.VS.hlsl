#include "GPUParticle.hlsli"
#include "GPUParticleGlow.hlsli"

// 発光パスの頂点シェーダー: 粒を、熱さ・速度の向きの尾・ちらつきつきの板として並べる。
// 通常の描画(GPUParticle.VS)と同じ粒のバッファを読む。冷めきった粒や、死んだ粒は、大きさ 0 で潰して描かない。

struct PerView {
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

ConstantBuffer<PerView> gPerView : register(b0);
StructuredBuffer<GPUParticle> gParticles : register(t0);
ConstantBuffer<GlowParams> gGlow : register(b1);

// ちらつきの位相を粒ごとにずらす角度(黄金角。隣り合う粒の位相が揃わない)
static const float32_t kPhaseStep = 2.399963f;
// 長さがこれ以下のベクトルは、向きが決められない
static const float32_t kMinLength = 0.0001f;

struct VertexShaderInput {
    float32_t3 position : POSITION;
    float32_t2 texcoord : TEXCOORD;
};

struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD;
    float32_t heat : HEAT;
    float32_t brightness : BRIGHTNESS;
    float32_t3 tint : TINT;       // 粒自身の色(ownColor のとき、熱さの色の代わりに使う)
    float32_t ownColor : OWNCOLOR; // 1 なら粒自身の色で光らせる(glowGain の符号が負)
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    output.position = float32_t4(0.0f, 0.0f, 0.0f, 0.0f); // w = 0 で、クリップされて描かれない
    output.texcoord = float32_t2(0.0f, 0.0f);
    output.heat = 0.0f;
    output.brightness = 0.0f;
    output.tint = float32_t3(0.0f, 0.0f, 0.0f);
    output.ownColor = 0.0f;

    GPUParticle particle = gParticles[instanceId];
    if (particle.color.a <= 0.0f || particle.scale.x <= 0.0f) {
        return output;
    }

    // 熱さ: 出たての粒は高く、冷めると下がる。巣に近い粒は熱を帯び(集まりが進むほど強く)、速い粒も熱い
    float32_t distanceToCenter = length(particle.translate - gGlow.hotCenter);
    float32_t nearness = saturate(1.0f - distanceToCenter / max(gGlow.hotRadius, kMinLength));
    float32_t speed = length(particle.velocity);
    float32_t heat = saturate(particle.heat + gGlow.hotBoost * nearness + speed * gGlow.speedHeatGain);

    // 画面の右・上の向き(ビルボード行列の行)で、速度を画面上の向きに直し、板を速度の向きに伸ばす(光の尾)
    float32_t3 right = gPerView.billboardMatrix[0].xyz;
    float32_t3 up = gPerView.billboardMatrix[1].xyz;
    float32_t2 screenVelocity = float32_t2(dot(particle.velocity, right), dot(particle.velocity, up));
    float32_t screenSpeed = length(screenVelocity);
    float32_t2 direction = screenSpeed > kMinLength ? screenVelocity / screenSpeed : float32_t2(1.0f, 0.0f);
    float32_t stretch = min(1.0f + gGlow.streakLength * screenSpeed, gGlow.streakMax);

    float32_t size = particle.scale.x;
    float32_t2 local = float32_t2(input.position.x * stretch, input.position.y) * size;
    float32_t3 offset = right * (local.x * direction.x - local.y * direction.y) + up * (local.x * direction.y + local.y * direction.x);
    float32_t4 clip = mul(float32_t4(particle.translate + offset, 1.0f), gPerView.viewProjection);

    // ちらつき(粒ごとに位相をずらす)と、遠いほど暗くする減衰
    float32_t twinkle = max(0.0f, 1.0f + gGlow.twinkleAmount * sin(gGlow.time * gGlow.twinkleSpeed + float32_t(instanceId) * kPhaseStep));
    float32_t attenuation = exp(-gGlow.distanceFalloff * max(clip.w, 0.0f));

    output.position = clip;
    output.texcoord = input.texcoord;
    output.heat = heat;
    // glowGain の符号が負なら、粒自身の色で光らせる印(明るさは絶対値)
    output.brightness = particle.color.a * abs(particle.glowGain) * twinkle * attenuation * gGlow.intensity;
    output.tint = particle.color.rgb;
    output.ownColor = particle.glowGain < 0.0f ? 1.0f : 0.0f;
    return output;
}
