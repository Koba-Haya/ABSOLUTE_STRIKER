#include "GPUParticle.hlsli"

ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<FieldArray> gFields : register(b2);
RWStructuredBuffer<GPUParticle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

static const uint32_t kMaxParticles = 1024;
// ゆらぎ(Turbulence)の位相をずらす定数。軸ごとに違う値にして、3 軸の動きが同じ波にならないようにする
static const float32_t3 kTurbulencePhase = float32_t3(1.7f, 4.1f, 2.3f);
static const float32_t3 kTurbulenceFrequencyScale = float32_t3(1.0f, 1.31f, 0.77f);

// CG3で学んだFieldの概念をGPU Particleに適用する。
// Attractor: targetへ吸い寄せる / Wind: directionへ一定の力 / Vortex: direction軸周りに渦を巻かせる /
// Turbulence: 位置と時間で変わるゆらぎの力(漂う動き)。
// 加速度を返す。Drag(速度に比例した減速)は加速度ではないので、dragOut に減衰率を足し込んで返す
float32_t3 ApplyFields(float32_t3 position, float32_t time, out float32_t dragOut) {
    float32_t3 accel = float32_t3(0.0f, 0.0f, 0.0f);
    dragOut = 0.0f;
    for (uint32_t i = 0; i < gFields.count; ++i) {
        Field f = gFields.fields[i];
        if (f.type == 1) {
            // Attractor
            float32_t3 toTarget = f.target - position;
            float32_t dist = length(toTarget);
            if (dist > 0.0001f) {
                accel += (toTarget / dist) * f.strength;
            }
        } else if (f.type == 2) {
            // Wind
            float32_t3 dir = f.direction;
            float32_t len = length(dir);
            if (len > 0.0001f) {
                accel += (dir / len) * f.strength;
            }
        } else if (f.type == 3) {
            // Vortex: direction軸を中心にtargetからの相対位置に接線方向の力を与える
            float32_t3 axis = f.direction;
            float32_t axisLen = length(axis);
            if (axisLen > 0.0001f) {
                axis /= axisLen;
                float32_t3 toParticle = position - f.target;
                float32_t3 tangent = cross(axis, toParticle);
                float32_t tangentLen = length(tangent);
                if (tangentLen > 0.0001f) {
                    accel += (tangent / tangentLen) * f.strength;
                }
            }
        } else if (f.type == 4) {
            // Drag: strength が 1 秒あたりの減衰率
            dragOut += f.strength;
        } else if (f.type == 6) {
            // Spring: targetへの距離に比例した力(遠い粒ほど強く引かれる)。Drag と組むと、どの距離からでも同じような時間で収束する
            accel += (f.target - position) * f.strength;
        } else if (f.type == 5) {
            // Turbulence: 位置(空間の細かさ direction.x)と時間(変化の速さ direction.y)で変わる、各軸が -1 〜 1 の力。
            // 軸ごとに、別の軸の位置で sin を取り、位相と周波数をずらすことで、3 軸が独立に揺れて見えるようにする
            float32_t3 phase = position.yzx * f.direction.x * kTurbulenceFrequencyScale + time * f.direction.y + kTurbulencePhase;
            accel += float32_t3(sin(phase.x), sin(phase.y), sin(phase.z)) * f.strength;
        }
    }
    return accel;
}

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID) {
    uint32_t particleIndex = DTid.x;

    // 全消去のフレーム: InitializeGPUParticle.CS と同じ状態に戻す(粒をゼロにし、空きの一覧を作り直す)
    if (gPerFrame.clearAll != 0) {
        if (particleIndex < kMaxParticles) {
            gParticles[particleIndex] = (GPUParticle)0;
            gFreeList[particleIndex] = particleIndex;
        }
        if (particleIndex == 0) {
            gFreeListIndex[0] = kMaxParticles - 1;
        }
        return;
    }

    if (particleIndex < kMaxParticles) {
        // alphaが0のparticleは死んでいるとみなして更新しない
        if (gParticles[particleIndex].color.a != 0.0f) {
            // Fieldによる加速度を速度に積算する
            float32_t drag;
            float32_t3 accel = ApplyFields(gParticles[particleIndex].translate, gPerFrame.time, drag);
            gParticles[particleIndex].velocity += accel * gPerFrame.deltaTime;
            // 速度に比例した減速(フレームレートに依存しない指数的な減衰)
            gParticles[particleIndex].velocity *= exp(-drag * gPerFrame.deltaTime);

            // 移動処理（フレームレートに依存しないよう deltaTime を乗算）
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;

            // 寿命進行処理
            gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

            // 経過割合に応じて、色を colorStart から colorEnd へ変え、アルファ（フェードアウト）を下げる。
            // fadePower が 1 なら線形、大きいほど終わり際まで残ってから消える。色が変わらない粒(従来)は、colorStart == colorEnd
            float32_t age = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            float32_t cool = saturate(age * gParticles[particleIndex].coolRate);
            gParticles[particleIndex].color.rgb = lerp(gParticles[particleIndex].colorStart, gParticles[particleIndex].colorEnd, cool);
            // 熱さ: 冷めるほど下がる(発光パスが、色の傾きと明るさに使う)
            // (発光専用の粒は、冷めずに、熱さを colorStart.x で直接指定する。核の光の強さの表現に使う)
            gParticles[particleIndex].heat = gParticles[particleIndex].glowOnly > 0.5f ? gParticles[particleIndex].colorStart.x : 1.0f - cool;
            gParticles[particleIndex].color.a = saturate(1.0f - pow(age, gParticles[particleIndex].fadePower));

            // alphaが0になった（寿命を迎えた）らFreeListに戻す
            if (gParticles[particleIndex].color.a == 0.0f) {
                // スケールを0にして頂点シェーダ側で描画を無効化
                gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);

                int32_t freeListIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

                // 最新のFreeListIndexの場所に死んだParticleのIndexを設定する
                if ((freeListIndex + 1) < kMaxParticles) {
                    gFreeList[freeListIndex + 1] = particleIndex;
                } else {
                    // 通常到達しない安全策
                    InterlockedAdd(gFreeListIndex[0], -1);
                }
            }
        }
    }
}
