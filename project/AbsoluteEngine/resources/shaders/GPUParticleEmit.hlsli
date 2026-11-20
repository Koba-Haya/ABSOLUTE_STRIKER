#include "GPUParticle.hlsli"

// 粒を出す計算シェーダー(EmitParticle.CS、SurfaceEmitParticle.CS)が共有する、粒の初期化。
// 分けた理由: スタイル付きの粒の「大きさ・向き・速さ・寿命・色」の決め方を、2 つのシェーダーに写さず、1 か所にだけ持つため。

// 色が変わりきる寿命の割合の下限(0 で割らないため)
static const float32_t kMinCoolRatio = 0.01f;
// 長さがこれ以下の向きは、正規化できないので使わない
static const float32_t kMinDirectionLength = 0.0001f;

// スタイル付きの粒の、位置と経過時間以外を決める。エミッタ e の指定の範囲で、ランダムに決める
// (一様な大きさ、向きはランダムで、速さを範囲から選ぶ)。
// outwardVelocity: 全ての粒に足す速度(e.baseVelocity に加えて)。粒ごとに違う速度(外向きなど)を足したいときに使う
void InitStyledParticle(RWStructuredBuffer<GPUParticle> particles, uint32_t particleIndex, Emitter e, float32_t3 outwardVelocity, inout RandomGenerator generator) {
    float32_t size = lerp(e.sizeMin, e.sizeMax, generator.Generate1d());
    particles[particleIndex].scale = float32_t3(size, size, size);

    float32_t3 direction = generator.Generate3d() * 2.0f - 1.0f;
    float32_t directionLength = length(direction);
    direction = directionLength > kMinDirectionLength ? direction / directionLength : float32_t3(0.0f, 1.0f, 0.0f);
    float32_t speed = lerp(e.speedMin, e.speedMax, generator.Generate1d());
    particles[particleIndex].velocity = e.baseVelocity + outwardVelocity + direction * speed;

    particles[particleIndex].lifeTime = lerp(e.lifeMin, e.lifeMax, generator.Generate1d());
    particles[particleIndex].color = float32_t4(e.colorStart, 1.0f);
    particles[particleIndex].colorStart = e.colorStart;
    particles[particleIndex].colorEnd = e.colorEnd;
    particles[particleIndex].coolRate = 1.0f / max(e.coolRatio, kMinCoolRatio);
    particles[particleIndex].fadePower = e.fadePower;
    particles[particleIndex].heat = 1.0f;
    particles[particleIndex].glowOnly = e.glowOnly;
    particles[particleIndex].glowGain = e.glowGain;
}
