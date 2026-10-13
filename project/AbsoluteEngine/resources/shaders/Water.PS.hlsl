#include "Fullscreen.hlsli"
#include "Sky.hlsli"
#include "Water.hlsli"

cbuffer WaterParam : register(b0) {
    row_major float4x4 gInvViewProj;
    float32_t3 gCameraPosition;
    float32_t gTime;
    float32_t3 gSunDirection;
    float32_t gWrapLength;          // 描画用の座標を巻き戻す 1 回分の長さ(z)。巻き戻していなければ 0
    float32_t3 gLocalCameraPosition; // 巻き戻し後の(小さい)カメラ位置。gCameraPosition = これ + 巻き戻した分
    float32_t gWrapCount;           // 巻き戻した回数
};

// t0: 平面反射のテクスチャ(水面より上の物体を、水面を軸に上下反転して本編と同じカメラで描いたもの)。
// 物体の無い所は rgba = 0。物体の所は α = 1(縁だけ中間の値)
Texture2D<float32_t4> gReflection : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t3 kWaterDeepColor = float32_t3(0.00f, 0.08f, 0.17f); // 深い青(リニア)
static const float32_t kFresnelF0 = 0.02f;   // 真上から見たときの反射率(水は約 2%)
static const float32_t kFogDensity = 0.012f; // 霞の濃さ(距離に対する指数)
static const float32_t kGlitterPower = 256.0f;
static const float32_t kGlitterIntensity = 2.0f;
static const float32_t kReflectionNormalBlend = 0.35f; // 空の反射に使う法線の揺れの割合(0 で完全に平ら、1 で波のまま)
static const float32_t kReflectionDistortion = 0.03f;  // 物体の映り込みを波の法線でゆらす量(画面の UV 単位。0 で歪みなし)

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // 視線方向の復元は Sky.PS と同じ
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);
    float32_t4 nearPos = mul(float32_t4(ndc, 0.0f, 1.0f), gInvViewProj);
    float32_t4 farPos = mul(float32_t4(ndc, 1.0f, 1.0f), gInvViewProj);
    float32_t3 dir = normalize(farPos.xyz / farPos.w - nearPos.xyz / nearPos.w);
    float32_t3 sunDir = normalize(gSunDirection);

    // 水面に当たらない視線(地平線より上)は空に任せる
    clip(-dir.y);

    // 視線と水面(y = kWaterHeight)の交点。t はカメラからの距離
    float32_t t = (gCameraPosition.y - kWaterHeight) / -dir.y;
    float32_t2 hitXZ = gCameraPosition.xz + dir.xz * t;

    float32_t2 localHitXZ = gLocalCameraPosition.xz + dir.xz * t;
    float32_t3 normal = WaterNormal(hitXZ, localHitXZ, gWrapLength, gWrapCount, t, gTime);
    float32_t3 viewDir = -dir;

    // フレネル(Schlick): 視線が浅いほど反射が強い
    float32_t nDotV = saturate(dot(normal, viewDir));
    float32_t fresnel = kFresnelF0 + (1.0f - kFresnelF0) * pow(1.0f - nDotV, 5.0f);

    // 反射方向の空の色。水面より下を向く反射は空として無意味なので上向きに折り返す
    float32_t3 reflectDir = reflect(dir, normal);
    reflectDir.y = abs(reflectDir.y);

    // 空(雲)の映り込みは、波の細かい揺れで形が崩れて雲だと分からなくなるので、法線を平らに近づけた方向で調べる。
    // 太陽のきらめきには、波の法線そのままの反射方向を使う
    float32_t3 smoothNormal = normalize(lerp(float32_t3(0.0f, 1.0f, 0.0f), normal, kReflectionNormalBlend));
    float32_t3 skyReflectDir = reflect(dir, smoothNormal);
    skyReflectDir.y = abs(skyReflectDir.y);
    // 雲も映す: 反射の出発点は水面上の交点
    float32_t3 hitPosition = float32_t3(hitXZ.x, kWaterHeight, hitXZ.y);
    float32_t3 reflection = SkyColor(skyReflectDir, sunDir, hitPosition, gTime);

    // 物体(自機など)の映り込み: 反射テクスチャは本編と同じ画面座標で対応するので、同じ UV を波の法線で少しずらして読む。
    // 色は α を掛けた状態で保存されている(背景が 0 で塗られているため)ので、空の反射は (1 - α) だけ残して足す。
    // 物体の無い所は α = 0 なので、これまでの空だけの見た目と同じになる
    float32_t2 reflectionUV = input.texcoord + normal.xz * kReflectionDistortion;
    float32_t4 objectReflection = gReflection.Sample(gSampler, reflectionUV);
    reflection = objectReflection.rgb + reflection * (1.0f - objectReflection.a);

    // 太陽の照り返し(波の揺れできらめく)
    float32_t glitter = pow(saturate(dot(reflectDir, sunDir)), kGlitterPower);

    float32_t3 color = lerp(kWaterDeepColor, reflection, fresnel);
    color += kSunColor * glitter * kGlitterIntensity;

    // 遠方は地平線の空の色へ霞でなじませる(地平線で空と連続する)
    float32_t3 horizonDir = normalize(float32_t3(dir.x, 0.0f, dir.z));
    float32_t fog = 1.0f - exp(-t * kFogDensity);
    color = lerp(color, SkyBackground(horizonDir, sunDir), fog);

    output.color = float32_t4(color, 1.0f);
    return output;
}
