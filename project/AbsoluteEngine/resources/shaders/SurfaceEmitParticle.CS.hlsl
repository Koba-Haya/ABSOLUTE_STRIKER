#include "GPUParticleEmit.hlsli"
#include "CrackVoronoi.hlsli"

// モデルの頂点から粒を出す計算シェーダー(撃破演出で、ボロノイのセルが消える場所から、粒を出す)。
// 1 スレッドが 1 頂点を担当する。その頂点が属するセルが、今フレームの間(prevBreak の後、curBreak まで)に消えるなら、
// その頂点の、スキニングした後のワールド位置から、粒を 1 つ出す(emitChance の確率で間引く)。
// セルの形と、消える時刻は、描画(Object3d.PS)と同じ CrackVoronoi.hlsli で計算する。だから、セルが消える場所と、粒が出る場所が一致する

// スキニングの入力(Skinning.CS と同じ並び)
struct Well {
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Vertex {
    float4 position; // モデル本来の位置(スキニング前)
    float2 texcoord;
    float3 normal;
    float3 pad;
};

struct VertexInfluence {
    uint4 boneIDs;
    float4 weights;
};

// GPUSurfaceEmitParams(GPUParticle.h)と 1:1 で並びを合わせること。256 バイト
struct SurfaceEmitParams {
    Emitter style;             // 粒の見た目と寿命・速さの範囲(translate、radius などは使わない)
    float4x4 world;            // モデル空間からワールドへ
    float3 crackOrigin;        // ひびが広がり始める位置(モデル本来の座標)
    float crackCellScale;      // 1 / セルの大きさ
    float crackSpreadRadius;   // origin から、いちばん遠い所までの距離
    float prevBreak;           // 前のフレームの crackBreak
    float curBreak;            // 今のフレームの crackBreak
    float emitChance;          // 消える頂点のうち、粒を出す割合(0 〜 1)
    float outwardSpeed;        // origin から外向きに足す速さ
    uint vertexCount;
    float2 pad;
};

ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<SurfaceEmitParams> gSurface : register(b3);
RWStructuredBuffer<GPUParticle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
StructuredBuffer<Vertex> gVertices : register(t0);
StructuredBuffer<VertexInfluence> gInfluences : register(t1);
StructuredBuffer<Well> gPalette : register(t2);

static const uint kThreadsPerGroup = 64;
// 乱数の種の時間を折り返す周期(秒)と、種の大きさの倍率(EmitParticle.CS と同じ理由: sin の引数を、精度の保てる範囲に収める)
static const float kSeedTimePeriod = 64.0f;
static const float kSeedScale = 0.37f;
// 頂点の番号から、種の 2 つ目の成分を作る倍率(番号が近い頂点で、乱数が似ないようにする)
static const float kSeedIndexMix = 0.618f;

[numthreads(kThreadsPerGroup, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    // 全消去のフレームは、何も出さない(Update が全ての粒と空きの管理を戻す)
    if (gPerFrame.clearAll != 0) {
        return;
    }
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gSurface.vertexCount) {
        return;
    }

    Vertex vertex = gVertices[vertexIndex];
    float3 bindPosition = vertex.position.xyz;

    // この頂点のセルが消える時刻が、今フレームの間に入るか(描画と同じ計算)
    float3 nearestFeature;
    float3 nearestId;
    CrackEdgeDistance(bindPosition * gSurface.crackCellScale, nearestFeature, nearestId);
    float heatStart = CrackCellHeatStart(nearestFeature, nearestId, gSurface.crackCellScale, gSurface.crackOrigin, gSurface.crackSpreadRadius);
    float vanishTime = heatStart + CRACK_BREAK_HEAT_WINDOW;
    if (!(gSurface.prevBreak < vanishTime && vanishTime <= gSurface.curBreak)) {
        return;
    }

    // 間引く(粒の数を、emitChance で決める)
    RandomGenerator generator;
    float seedTime = fmod(gPerFrame.time, kSeedTimePeriod);
    generator.seed = float3(float(vertexIndex), float(vertexIndex) * kSeedIndexMix, seedTime) * kSeedScale;
    if (generator.Generate1d() >= gSurface.emitChance) {
        return;
    }

    // スキニングした後の位置(Skinning.CS と同じ計算)を、ワールドへ移す
    VertexInfluence influence = gInfluences[vertexIndex];
    float4x4 skinMatrix =
        influence.weights.x * gPalette[influence.boneIDs.x].skeletonSpaceMatrix +
        influence.weights.y * gPalette[influence.boneIDs.y].skeletonSpaceMatrix +
        influence.weights.z * gPalette[influence.boneIDs.z].skeletonSpaceMatrix +
        influence.weights.w * gPalette[influence.boneIDs.w].skeletonSpaceMatrix;
    float3 skinnedPosition = mul(float4(bindPosition, 1.0f), skinMatrix).xyz;
    float3 worldPosition = mul(float4(skinnedPosition, 1.0f), gSurface.world).xyz;

    // 外向きの速さ: origin(ワールド)から、粒の位置へ向かう向き
    float3 worldOrigin = mul(float4(gSurface.crackOrigin, 1.0f), gSurface.world).xyz;
    float3 outward = worldPosition - worldOrigin;
    float outwardLength = length(outward);
    outward = outwardLength > kMinDirectionLength ? outward / outwardLength : float3(0.0f, 1.0f, 0.0f);

    // 空きの粒を 1 つ取る(EmitParticle.CS と同じ手順)
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (0 <= freeListIndex && freeListIndex < kMaxParticles) {
        uint particleIndex = gFreeList[freeListIndex];
        gParticles[particleIndex].translate = worldPosition;
        InitStyledParticle(gParticles, particleIndex, gSurface.style, outward * gSurface.outwardSpeed, generator);
        gParticles[particleIndex].currentTime = 0.0f;
    } else {
        // 取れなかったので、減らしてしまった分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
