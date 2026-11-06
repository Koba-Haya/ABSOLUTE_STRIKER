#include "GPUParticle.hlsli"

ConstantBuffer<EmitterArray> gEmitters : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<GPUParticle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

static const uint32_t kMaxParticles = 1024;
static const uint32_t kThreadsPerEmitter = 64;
// 乱数の種の時間を折り返す周期(秒)と、種の大きさの倍率(sin の引数を、精度の保てる範囲に収める)
static const float32_t kSeedTimePeriod = 64.0f;
static const float32_t kSeedScale = 0.37f;
// 色が変わりきる寿命の割合の下限(0 で割らないため)
static const float32_t kMinCoolRatio = 0.01f;
// 長さがこれ以下の向きは、正規化できないので使わない
static const float32_t kMinDirectionLength = 0.0001f;

// エミッタ1つにつき1スレッドグループ(64スレッド)を割り当てて並列化する。
// 1グループのスレッド数を超えるcountはストライドループで担当する。
[numthreads(kThreadsPerEmitter, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t emitterIndex = DTid.x / kThreadsPerEmitter;
    uint32_t localIndex = DTid.x % kThreadsPerEmitter;

    // 全消去のフレームは、何も出さない(Update が全ての粒と空きの管理を戻す)
    if (gPerFrame.clearAll != 0) {
        return;
    }

    Emitter e = gEmitters.emitters[emitterIndex];
    if (e.enabled == 0 || e.emit == 0) {
        return;
    }

    RandomGenerator generator;
    // シード値の設定（グローバルスレッドIDと時間を組み合わせる）
    // 種は、時間が増えても小さな値にとどめる。起動からの経過時間に比例する値のまま sin に渡すと、時間がたつほど引数が巨大になり、
    // 単精度の sin の精度が落ちて、乱数が偏る(粒が同じ所・同じ向きに固まる)ため。時間は周期 kSeedTimePeriod で折り返す
    float32_t seedTime = fmod(gPerFrame.time, kSeedTimePeriod);
    generator.seed = (float32_t3(DTid) + seedTime) * kSeedScale;

    for (uint32_t countIndex = localIndex; countIndex < e.count; countIndex += kThreadsPerEmitter) {
        int32_t freeListIndex;
        // FreeListのIndexを1つ前に設定し、現在のIndexを取得する
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        if (0 <= freeListIndex && freeListIndex < kMaxParticles) {
            uint32_t particleIndex = gFreeList[freeListIndex];

            if (e.styled == 0) {
                // 従来: 色・大きさ・速さ・寿命を、決まった範囲でランダムに決める
                gParticles[particleIndex].scale = generator.Generate3d() * 0.3f + 0.1f;
            }

            // エミッターの形状に応じてランダム配置
            float32_t3 randPos = generator.Generate3d() * 2.0f - 1.0f;
            if (e.shape == 1) {
                // Box: 半径ではなく半径ベクトル(halfExtents)でスケールする
                gParticles[particleIndex].translate = e.translate + randPos * e.halfExtents;
            } else {
                // Sphere
                gParticles[particleIndex].translate = e.translate + randPos * e.radius;
            }

            if (e.styled == 0) {
                gParticles[particleIndex].color.rgb = generator.Generate3d();
                gParticles[particleIndex].color.a = 1.0f;

                // ランダムな速度と寿命
                gParticles[particleIndex].velocity = (generator.Generate3d() * 2.0f - 1.0f) * 2.0f;
                gParticles[particleIndex].lifeTime = 1.0f + generator.Generate1d() * 2.0f;

                // 色は変わらず、アルファは線形に下がる
                gParticles[particleIndex].colorStart = gParticles[particleIndex].color.rgb;
                gParticles[particleIndex].colorEnd = gParticles[particleIndex].color.rgb;
                gParticles[particleIndex].coolRate = 1.0f;
                gParticles[particleIndex].fadePower = 1.0f;
                gParticles[particleIndex].heat = 1.0f;
                gParticles[particleIndex].glowOnly = 0.0f;
                gParticles[particleIndex].glowGain = 1.0f;
            } else {
                // スタイル付き: エミッタの指定の範囲でランダムに決める(一様な大きさ、向きはランダムで、速さを範囲から選ぶ)
                float32_t size = lerp(e.sizeMin, e.sizeMax, generator.Generate1d());
                gParticles[particleIndex].scale = float32_t3(size, size, size);

                float32_t3 direction = generator.Generate3d() * 2.0f - 1.0f;
                float32_t directionLength = length(direction);
                direction = directionLength > kMinDirectionLength ? direction / directionLength : float32_t3(0.0f, 1.0f, 0.0f);
                float32_t speed = lerp(e.speedMin, e.speedMax, generator.Generate1d());
                gParticles[particleIndex].velocity = e.baseVelocity + direction * speed;

                gParticles[particleIndex].lifeTime = lerp(e.lifeMin, e.lifeMax, generator.Generate1d());
                gParticles[particleIndex].color = float32_t4(e.colorStart, 1.0f);
                gParticles[particleIndex].colorStart = e.colorStart;
                gParticles[particleIndex].colorEnd = e.colorEnd;
                gParticles[particleIndex].coolRate = 1.0f / max(e.coolRatio, kMinCoolRatio);
                gParticles[particleIndex].fadePower = e.fadePower;
                gParticles[particleIndex].heat = 1.0f;
                gParticles[particleIndex].glowOnly = e.glowOnly;
                gParticles[particleIndex].glowGain = e.glowGain;
            }
            gParticles[particleIndex].currentTime = 0.0f;
        } else {
            // 発生させられなかったので、減らしてしまった分もとに戻す
            InterlockedAdd(gFreeListIndex[0], 1);
            break;
        }
    }
}
