#include "CrackVoronoi.hlsli"

// ボロノイのひびの線の上にある頂点を、まばらに選ぶ計算シェーダー(撃破演出の、光線の出る点を決めるために使う)。
// 1 スレッドが 1 頂点を担当する。頂点が、ひびの線の上にあり(描画の Object3d.PS が光らせる範囲と同じ判定)、
// 確率 chance で選ばれたら、その頂点の番号と、その頂点のセルが「光り始める時刻」「消える時刻」(crackBreak の値)を、出力バッファの末尾へ追加する。
// 時刻は、描画(Object3d.PS)と同じ計算(CrackCellHeatStart)なので、セルが実際に光って消える瞬間と一致する。
// セルの計算は、描画(Object3d.PS)や、粒を出す計算(SurfaceEmitParticle.CS)と共有する CrackVoronoi.hlsli を使う。
// だから、ひびが光る場所と、選ばれる点が一致する(CPU へセルの計算を写さない)。

struct Vertex {
    float4 position; // モデル本来の位置(スキニング前)
    float2 texcoord;
    float3 normal;
    float3 pad;
};

// CrackPointPickParams(CrackPointPicker.h)と 1:1 で並びを合わせること
struct PickParams {
    float crackCellScale; // 1 / セルの大きさ
    float maxEdge;        // ひびの線とみなす、境目からの距離の上限(セルの大きさに対する割合。描画のひびの線の太さと同じ値)
    float chance;         // ひびの線の上の頂点のうち、選ぶ割合(0 〜 1)
    float seed;           // 乱数の種(依頼ごとに変える)
    uint vertexCount;
    uint maxPoints;       // 出力バッファに書ける点の数
    float2 pad;
    float3 crackOrigin;       // ひびが広がり始める位置(モデル本来の座標。セルが消える時刻の計算に使う)
    float crackSpreadRadius;  // origin から、いちばん遠い所までの距離
};

ConstantBuffer<PickParams> gPick : register(b0);
StructuredBuffer<Vertex> gVertices : register(t0);
// [0] = 選ばれた点の数(maxPoints を超えることもある。読む側が上限で切る)。
// [1 以降] = 点ごとに kUintsPerPoint 個: 頂点の番号、セルが光り始める時刻(float のビット列)、セルが消える時刻(float のビット列)
RWStructuredBuffer<uint> gOutput : register(u0);

static const uint kThreadsPerGroup = 64;
// 1 点あたりの出力の数(CrackPointPicker.h の kUintsPerPoint と同じ値にすること)
static const uint kUintsPerPoint = 3;
// 頂点の番号と種から、0 〜 1 の乱数を作る(同じ入力なら同じ値)
static const float2 kRandomMix = float2(12.9898f, 78.233f);
static const float kRandomScale = 43758.5453f;

[numthreads(kThreadsPerGroup, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gPick.vertexCount) {
        return;
    }

    float3 bindPosition = gVertices[vertexIndex].position.xyz;
    float3 nearestFeature;
    float3 nearestId;
    float edge = CrackEdgeDistance(bindPosition * gPick.crackCellScale, nearestFeature, nearestId);
    if (edge > gPick.maxEdge) {
        return; // ひびの線の上ではない
    }

    float random = frac(sin(dot(float2(float(vertexIndex), gPick.seed), kRandomMix)) * kRandomScale);
    if (random >= gPick.chance) {
        return;
    }

    uint slot;
    InterlockedAdd(gOutput[0], 1, slot);
    if (slot < gPick.maxPoints) {
        float heatStart = CrackCellHeatStart(nearestFeature, nearestId, gPick.crackCellScale, gPick.crackOrigin, gPick.crackSpreadRadius);
        uint base = 1 + slot * kUintsPerPoint;
        gOutput[base] = vertexIndex;
        gOutput[base + 1] = asuint(heatStart);
        gOutput[base + 2] = asuint(heatStart + CRACK_BREAK_HEAT_WINDOW);
    }
}
