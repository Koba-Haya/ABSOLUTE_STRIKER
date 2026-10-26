#include "Fullscreen.hlsli"
#include "Noise.hlsli"
#include "LensMist.hlsli"
#include "TransitionParam.hlsli"

// 画面遷移の最終パスの 2 つ目: レンズに付いた水滴。
// 1 つ目のパス(Transition.PS.hlsl)の結果(霧や雲がかかった画面)を読み、水滴の形に沿って屈折させて描く。
// 水滴の形は LensMist.hlsli(小粒、流れる大粒、跡)。霧や雲の計算をやり直さないので、どの遷移の種類にも足せる。
// 水滴を使わない遷移、または濡れ具合が 0 の間は、画面をそのまま写す。

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

// 小粒の水滴の見た目(形は LensMist.hlsli)
static const uint32_t kMistDropLayers = 3u;             // 大きさの違う層の数(層ごとに、格子を回す角度も変える)
static const float32_t kMistDropCells[3] = {200.0f, 120.0f, 64.0f}; // 層ごとの、画面の高さあたりのセル数(細かい層から)
static const int32_t kMistDropSeeds[3] = {277, 5, 211};             // 層ごとの乱数の種
static const float32_t kMistDropRotations[3] = {0.72f, 0.30f, -0.40f}; // 層ごとの、格子を回す角度(ラジアン。約 41、17、-23 度)
static const float32_t kMistDropRefract = 0.9f;         // 水滴の中の像のずれ(水滴の半径に対する倍率。像は反転して見える)
static const float32_t kMistDropBodyDarken = 0.82f;     // 水滴の中の明るさ(暗い点に見える)
static const float32_t kMistDropRimStart = 0.55f;       // 縁が暗くなり始める位置(半径に対する割合)
static const float32_t kMistDropRimDarken = 0.45f;
static const float32_t2 kMistDropSpecPos = float32_t2(-0.38f, -0.42f); // 小さなハイライトの位置(中心からの割合。上が負)
static const float32_t kMistDropSpecSize = 0.30f;
static const float32_t kMistDropSpecGain = 0.55f;
static const float32_t kRunDropRefract = 0.9f;          // 流れる大粒: 像のずれ(半径に対する倍率)
static const float32_t kRunDropBodyDarken = 0.9f;       // 大粒の中の明るさ(小粒より少し明るい)
static const float32_t kRunDropRimDarken = 0.5f;
static const float32_t kRunDropSpecGain = 0.75f;
static const float32_t kTrailRefract = 1.0f;            // 跡: 横向きの像のずれ(幅に対する倍率)
static const float32_t kTrailBodyDarken = 0.93f;
static const float32_t kTrailRimDarken = 0.55f;
static const float32_t kTrailLift = 0.10f;              // 跡の中央の、わずかな明るさ
static const float32_t kLensEnabledThreshold = 0.5f;    // gMistDrop.w がこれを超えたら、水滴を描く

// 画面をサンプルする(画面の外は端に張り付ける)
float32_t3 SampleScene(float32_t2 uv) {
    return gTexture.Sample(gSampler, saturate(uv)).rgb;
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    float32_t3 color = SampleScene(uv);
    float32_t wetness = gMistDrop.x;

    // 画素の微分(fwidth)を使う関数は、分岐の外から呼ぶ必要があるので、水滴の形は常に求める
    DropletHit smallHits[3];
    [unroll]
    for (uint32_t layer = 0; layer < kMistDropLayers; ++layer) {
        smallHits[layer] = SmallDropletLayer(uv, gAspect, wetness, kMistDropCells[layer], kMistDropSeeds[layer], kMistDropRotations[layer]);
    }
    DropletHit runDrop;
    DropletHit runTrail;
    RunningDropletLayer(uv, gAspect, wetness, gMistDrop.z, runDrop, runTrail);

    if (gMistDrop.w > kLensEnabledThreshold && wetness > 0.0f) {
        [unroll]
        for (uint32_t layer = 0; layer < kMistDropLayers; ++layer) {
            DropletHit hit = smallHits[layer];
            if (hit.mask > 0.0f) {
                // 水滴はレンズ: 中の像は反転し、少しずれて見える。縁は暗く、上に小さなハイライト
                float32_t2 offset = -hit.normal * hit.radiusUv * kMistDropRefract;
                offset.x /= gAspect;
                float32_t3 inside = SampleScene(uv + offset) * kMistDropBodyDarken;
                float32_t rim = smoothstep(kMistDropRimStart, 1.0f, length(hit.normal));
                inside *= 1.0f - kMistDropRimDarken * rim;
                float32_t spec = smoothstep(kMistDropSpecSize, 0.0f, length(hit.normal - kMistDropSpecPos));
                inside += spec * kMistDropSpecGain;
                color = lerp(color, inside, hit.mask);
            }
        }

        // 流れる大粒: 先に跡、その上に水滴
        if (runTrail.mask > 0.0f) {
            float32_t2 offset = -runTrail.normal * runTrail.radiusUv * kTrailRefract;
            offset.x /= gAspect;
            float32_t3 inside = SampleScene(uv + offset) * kTrailBodyDarken;
            float32_t rim = smoothstep(kMistDropRimStart, 1.0f, length(runTrail.normal));
            inside = inside * (1.0f - kTrailRimDarken * rim) + kTrailLift * (1.0f - rim);
            color = lerp(color, inside, runTrail.mask);
        }
        if (runDrop.mask > 0.0f) {
            float32_t2 offset = -runDrop.normal * runDrop.radiusUv * kRunDropRefract;
            offset.x /= gAspect;
            float32_t3 inside = SampleScene(uv + offset) * kRunDropBodyDarken;
            float32_t rim = smoothstep(kMistDropRimStart, 1.0f, length(runDrop.normal));
            inside *= 1.0f - kRunDropRimDarken * rim;
            float32_t spec = smoothstep(kMistDropSpecSize, 0.0f, length(runDrop.normal - kMistDropSpecPos));
            inside += spec * kRunDropSpecGain;
            color = lerp(color, inside, runDrop.mask);
        }
    }

    output.color = float32_t4(color, 1.0f);
    return output;
}
