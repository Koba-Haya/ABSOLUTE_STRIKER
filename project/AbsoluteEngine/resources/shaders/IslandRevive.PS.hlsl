#include "object3d.hlsli"

// 島が蘇る見た目(次の島の解放)のピクセルシェーダー。Object3d.PS の責務を増やさないよう、専用に分けてある。
//
// 見た目: 島は、はじめ「くすんだ色」(彩度を落とし、暗く、褐色に寄せた色)。島の上面の中心(蘇りの中心)から、波面が表面を広がり、
//         波面の内側は、元の色(生きた色)に変わる。波面の縁は、金色に光る。縁は、ノイズで乱れて、まだらに蘇る。
// 光の当たり方: Object3d.PS の Half-Lambert(平行光だけ)と同じ。ステージの島と、同じ明るさになる。
//
// 値の渡し方(Object3d.PS と同じ Material の並びのまま、島では使わないひび割れ用の欄を読み替える。ModelInstance::SetCrackParam で書く):
//   crackOrigin       : 蘇りの中心(モデル本来の座標。島の上面の中心)
//   crackSpreadRadius : 波面が最後に届く距離(中心から、島の最も遠い所まで。モデル本来の単位)
//   crackReveal       : 蘇った割合(0 = すべてくすんだ色、1 = すべて生きた色)
//   crackLineWidth    : 波面の縁の幅(モデル本来の単位。この幅の中で、金色に光りながら、色が変わる)
//   crackCellScale    : 縁の乱れ(ノイズ)の細かさ(1 / ノイズの 1 マスの大きさ。SetCrackParam の cellSize の逆数)
//   crackColor        : 縁の光の色
//   crackIntensity    : 縁の光の強さ
// 色に掛ける値は、Material の color(白なら、テクスチャの色のまま)。テクスチャは t0、サンプラーは s0。

#define MAX_DIR_LIGHTS 4

struct Material
{
    float4 color;
    int enableLighting;
    float3 specularColor;
    float4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    float2 pad;

    int enableDissolve;
    float dissolveThreshold;
    float dissolveEdgeRange;
    float dissolveEmber;
    float3 dissolveEdgeColor;
    float pad3;
    float3 dissolveMaskColor;
    float pad4;

    int enableCrack;
    float crackCellScale;
    float crackLineWidth;
    float crackReveal;
    float3 crackColor;
    float crackIntensity;
    float3 crackOrigin;
    float crackSpreadRadius;
    float crackBreak;
    float3 crackPad;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
    int enabled;
    float3 pad0;
};

struct DirectionalLightGroup
{
    int count;
    float3 padCount;
    DirectionalLight lights[MAX_DIR_LIGHTS];
    int enabled;
    float3 padEnabled;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLightGroup> gDirectionalLights : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// くすんだ色: 彩度を落とす割合(0 = 元の色、1 = 完全な灰色)、明るさの倍率、褐色に寄せる色(枯れた草と岩の色)
static const float DULL_DESATURATE = 0.90f;
static const float DULL_BRIGHTNESS = 0.20f;
static const float3 DULL_TINT = float3(0.95f, 0.90f, 0.85f);
// 輝度の重み(Rec.601)
static const float3 LUMA_WEIGHTS = float3(0.299f, 0.587f, 0.114f);
// 波面の縁の乱れの大きさ(波面が届く距離に対する割合)。大きいほど、縁がまだらになる
static const float FRONT_RAGGEDNESS = 0.15f;
// 縁の光の幅(縁の幅に対する倍率)。光は、縁の内側寄りに出る
static const float GLOW_WIDTH_RATIO = 0.6f;
static const float GLOW_CENTER_RATIO = 0.5f;
// 縁の光が消え始める、蘇った割合(蘇りきったあと、島に光が残らないように、終わりへ向けて消す)
static const float GLOW_FADE_START = 0.85f;
// 波面の幅の下限(0 だと、割り算が壊れる)
static const float MIN_FRONT_WIDTH = 0.0001f;

// 3 次元の値ノイズの、格子点の乱数(0 〜 1)
static float ReviveHash(float3 p)
{
    p = frac(p * 0.3183099f + 0.1f);
    p *= 17.0f;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3 次元の値ノイズ(0 〜 1)。格子点の乱数を、なめらかに補間する
static float ReviveNoise(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0f - 2.0f * f);
    float n000 = ReviveHash(i);
    float n100 = ReviveHash(i + float3(1, 0, 0));
    float n010 = ReviveHash(i + float3(0, 1, 0));
    float n110 = ReviveHash(i + float3(1, 1, 0));
    float n001 = ReviveHash(i + float3(0, 0, 1));
    float n101 = ReviveHash(i + float3(1, 0, 1));
    float n011 = ReviveHash(i + float3(0, 1, 1));
    float n111 = ReviveHash(i + float3(1, 1, 1));
    return lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y),
                lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
}

// 2 段重ねたノイズ(0 〜 約 1)
static float ReviveFbm(float3 p)
{
    return ReviveNoise(p) * 0.65f + ReviveNoise(p * 2.07f) * 0.35f;
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float3 albedo = gTexture.Sample(gSampler, input.texcoord).rgb * gMaterial.color.rgb;

    // くすんだ色: 彩度を落として、暗くし、褐色に寄せる
    float luma = dot(albedo, LUMA_WEIGHTS);
    float3 dull = lerp(albedo, float3(luma, luma, luma), DULL_DESATURATE) * DULL_BRIGHTNESS * DULL_TINT;

    // 蘇りの波面: 中心からの距離(縁を、ノイズで乱す)が、波面の位置より内側なら、生きた色
    float width = max(gMaterial.crackLineWidth, MIN_FRONT_WIDTH);
    float spread = gMaterial.crackSpreadRadius;
    float frontPosition = lerp(-width, spread + width, saturate(gMaterial.crackReveal)); // 0 のとき、島の外。1 のとき、島を越える
    float dist = length(input.localPosition - gMaterial.crackOrigin);
    float noise = ReviveFbm(input.localPosition * gMaterial.crackCellScale) - 0.5f;
    float distortedDist = dist + noise * spread * FRONT_RAGGEDNESS;
    float alive = 1.0f - smoothstep(frontPosition - width, frontPosition, distortedDist);
    float3 baseColor = lerp(dull, albedo, alive);

    // 光の当たり方(Half-Lambert。Object3d.PS と同じ)
    float3 N = normalize(input.normal);
    float dirGroupOn = (gDirectionalLights.enabled != 0) ? 1.0f : 0.0f;
    float3 lit = float3(0.0f, 0.0f, 0.0f);
    int dCount = min(gDirectionalLights.count, MAX_DIR_LIGHTS);
    [loop]
    for (int i = 0; i < dCount; ++i)
    {
        DirectionalLight dl = gDirectionalLights.lights[i];
        if (dl.enabled == 0)
            continue;
        float3 Ld = normalize(-dl.direction);
        float diff = pow(saturate(dot(N, Ld) * 0.5f + 0.5f), 2.0f);
        lit += baseColor * diff * dl.color.rgb * dl.intensity * dirGroupOn;
    }

    // 縁の金色の光(波面の縁のすぐ内側)。蘇りきる前に、消す
    float glowCenter = frontPosition - width * GLOW_CENTER_RATIO;
    float glowT = (distortedDist - glowCenter) / (width * GLOW_WIDTH_RATIO);
    float glow = exp(-glowT * glowT) * gMaterial.crackIntensity * (1.0f - smoothstep(GLOW_FADE_START, 1.0f, gMaterial.crackReveal));
    return float4(lit + gMaterial.crackColor * glow, 1.0f);
}
