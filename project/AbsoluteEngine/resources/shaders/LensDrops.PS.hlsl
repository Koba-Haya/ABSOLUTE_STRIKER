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
static const float32_t kMistDropRefract = 0.9f;         // 水滴の中の像のずれ(水滴の半径に対する倍率。像は反転して見える。さらに gLensEdge3.z を掛ける)
static const float32_t kMistDropRimStart = 0.55f;       // 跡の縁が暗くなり始める位置(半径に対する割合)
static const float32_t2 kMistDropSpecPos = float32_t2(-0.38f, -0.42f); // 小さなハイライトの位置(中心からの割合。上が負)
static const float32_t kMistDropSpecSize = 0.22f;
static const float32_t kMistDropSpecGain = 0.9f;
// 水滴の明るい縁と影(暗さでなく、光の当たり方で水滴らしさを出す)
static const float32_t kDropRimStart = 0.78f;           // 縁の帯が始まる位置(半径に対する割合。小さいほど太い)
static const float32_t2 kDropLightDir = float32_t2(-0.6f, -0.8f); // 光の来る向き(画面上。上が負)
static const float32_t kDropRimLightGain = 0.55f;       // 光の側の、明るい縁の強さ
static const float32_t kDropRimLightPower = 1.5f;       // 光の側に絞るべき(大きいほど、光の真正面だけ)
static const float32_t3 kDropShadowTint = float32_t3(0.78f, 0.88f, 1.0f); // 影の側に掛ける、青みのある色
static const float32_t kDropMinLength = 0.0001f;        // 向きを求めるときの長さの下限
static const float32_t kRunDropRefract = 0.9f;          // 流れる大粒: 像のずれ(半径に対する倍率。さらに gLensEdge3.z を掛ける)
static const float32_t kRunDropSpecGain = 1.1f;
static const float32_t kRunDropBodyBrighten = 0.03f;    // 大粒の中の明るさは、小粒より少し明るくする
static const float32_t kTrailRefract = 1.0f;            // 跡: 横向きの像のずれ(幅に対する倍率)
static const float32_t kTrailBodyDarken = 0.93f;
static const float32_t kTrailRimDarken = 0.55f;
static const float32_t kTrailLift = 0.10f;              // 跡の中央の、わずかな明るさ
static const float32_t kLensEnabledThreshold = 0.5f;    // gMistDrop.w がこれを超えたら、水滴を描く

// 縁のぼかし(縁の帯だけ、画面を軽くぼかす。水滴は、そのぼけた画面を屈折して映す)
static const uint32_t kEdgeBlurTaps = 8u;               // ぼかしのサンプル数(円盤状に散らす)
static const float32_t kEdgeBlurMinAmount = 0.01f;      // ぼかしの量がこれ未満なら、ぼかさない(サンプルを省く)
static const float32_t kGoldenAngle = 2.39996323f;      // 円盤状に散らすときの、サンプルごとの回転角(黄金角。ラジアン)
static const float32_t kTwoPi = 6.28318531f;
static const float32_t kScreenNoiseA = 52.9829189f;     // 画素ごとの回転(縞を砕く)に使う、インタリーブド勾配ノイズの定数
static const float32_t2 kScreenNoiseB = float32_t2(0.06711056f, 0.00583715f);

// 画面をサンプルする(画面の外は端に張り付ける)
float32_t3 SampleScene(float32_t2 uv) {
    return gTexture.Sample(gSampler, saturate(uv)).rgb;
}

// 縁のぼかし込みで画面をサンプルする。
//   ぼかしの量 = 縁の帯 x 濡れ具合(濡れていない間は、ぼかさない)。半径は gLensEdge2.y(画面の高さを 1 とする)。
//   分岐の中(水滴の屈折)から呼べるよう、ミップ 0 を直接読む(Sample は微分が必要で、分岐の中では不定になる)。
//   サンプルは、黄金角で円盤状に散らし、画素ごとに回転を変えて、縞を砕く。
//   pixel: 画素の位置(回転のノイズ用。屈折で読む位置ではなく、今の画素)
float32_t3 SampleSceneEdge(float32_t2 uv, float32_t2 pixel, float32_t wetness) {
    float32_t2 clamped = saturate(uv);
    float32_t amount = 0.0f;
    if (gLensEdge2.w > 0.5f) {
        amount = LensEdgeBand(clamped, gAspect, gLensEdge, gLensEdge2) * wetness;
    }
    if (amount < kEdgeBlurMinAmount || gLensEdge2.y <= 0.0f) {
        return gTexture.SampleLevel(gSampler, clamped, 0.0f).rgb;
    }
    float32_t radius = gLensEdge2.y * amount;
    float32_t startAngle = frac(kScreenNoiseA * frac(dot(pixel, kScreenNoiseB))) * kTwoPi;
    float32_t3 sum = float32_t3(0.0f, 0.0f, 0.0f);
    for (uint32_t i = 0; i < kEdgeBlurTaps; ++i) {
        float32_t angle = startAngle + float32_t(i) * kGoldenAngle;
        float32_t r = sqrt((float32_t(i) + 0.5f) / float32_t(kEdgeBlurTaps)) * radius;
        float32_t2 offset = float32_t2(cos(angle) / gAspect, sin(angle)) * r;
        sum += gTexture.SampleLevel(gSampler, saturate(clamped + offset), 0.0f).rgb;
    }
    return sum / float32_t(kEdgeBlurTaps);
}

// 水滴の中の色を整える。暗さでなく、光の当たり方で水滴らしさを出す(背景が白い雲でも、灰色の点にならない)。
//   inside: 屈折して読んだ背景の色  normal: 水滴の中の位置(-1〜1)  specGain: 小さなハイライトの強さ  bodyBrighten: 中の明るさへの加算
//   光の側: 細い縁が明るい(レンズが光を集める反射)。影の側: 縁が薄く暗くなり、わずかに青みを帯びる
float32_t3 ShadeDrop(float32_t3 inside, float32_t2 normal, float32_t specGain, float32_t bodyBrighten) {
    float32_t len = length(normal);
    float32_t2 dir = normal / max(len, kDropMinLength);
    float32_t facing = dot(dir, normalize(kDropLightDir)); // 1 = 光の側、-1 = 影の側
    float32_t rimBand = smoothstep(kDropRimStart, 1.0f, len);
    float32_t shadow = rimBand * saturate(-facing);
    float32_t lit = rimBand * pow(saturate(facing), kDropRimLightPower);
    inside *= min(gLensEdge4.x + bodyBrighten, 1.0f);
    inside *= 1.0f - gLensEdge4.y * shadow;
    inside = lerp(inside, inside * kDropShadowTint, shadow * gLensEdge4.z);
    inside += lit * kDropRimLightGain * gLensEdge3.w;
    float32_t spec = smoothstep(kMistDropSpecSize, 0.0f, length(normal - kMistDropSpecPos));
    inside += spec * specGain * gLensEdge3.w;
    return inside;
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    float32_t2 pixel = input.position.xy;
    float32_t wetness = gMistDrop.x;
    float32_t3 color = SampleSceneEdge(uv, pixel, wetness);

    // 画素の微分(fwidth)を使う関数は、分岐の外から呼ぶ必要があるので、水滴の形は常に求める
    DropletHit smallHits[3];
    [unroll]
    for (uint32_t layer = 0; layer < kMistDropLayers; ++layer) {
        smallHits[layer] = SmallDropletLayer(uv, gAspect, wetness, kMistDropCells[layer], kMistDropSeeds[layer], kMistDropRotations[layer], gLensEdge, gLensEdge2, gLensEdge3.x);
    }
    DropletHit runDrop;
    DropletHit runTrail;
    RunningDropletLayer(uv, gAspect, wetness, gMistDrop.z, gLensEdge, gLensEdge2, gLensEdge3.y, runDrop, runTrail);

    if (gMistDrop.w > kLensEnabledThreshold && wetness > 0.0f) {
        [unroll]
        for (uint32_t layer = 0; layer < kMistDropLayers; ++layer) {
            DropletHit hit = smallHits[layer];
            if (hit.mask > 0.0f) {
                // 水滴はレンズ: 中の像は反転し、強くずれて見える。縁と影の味付けは ShadeDrop
                float32_t2 offset = -hit.normal * hit.radiusUv * kMistDropRefract * gLensEdge3.z;
                offset.x /= gAspect;
                float32_t3 inside = ShadeDrop(SampleSceneEdge(uv + offset, pixel, wetness), hit.normal, kMistDropSpecGain, 0.0f);
                color = lerp(color, inside, hit.mask);
            }
        }

        // 流れる大粒: 先に跡、その上に水滴
        if (runTrail.mask > 0.0f) {
            float32_t2 offset = -runTrail.normal * runTrail.radiusUv * kTrailRefract;
            offset.x /= gAspect;
            float32_t3 inside = SampleSceneEdge(uv + offset, pixel, wetness) * kTrailBodyDarken;
            float32_t rim = smoothstep(kMistDropRimStart, 1.0f, length(runTrail.normal));
            inside = inside * (1.0f - kTrailRimDarken * rim) + kTrailLift * (1.0f - rim);
            color = lerp(color, inside, runTrail.mask);
        }
        if (runDrop.mask > 0.0f) {
            float32_t2 offset = -runDrop.normal * runDrop.radiusUv * kRunDropRefract * gLensEdge3.z;
            offset.x /= gAspect;
            float32_t3 inside = ShadeDrop(SampleSceneEdge(uv + offset, pixel, wetness), runDrop.normal, kRunDropSpecGain, kRunDropBodyBrighten);
            color = lerp(color, inside, runDrop.mask);
        }
    }

    output.color = float32_t4(color, 1.0f);
    return output;
}
