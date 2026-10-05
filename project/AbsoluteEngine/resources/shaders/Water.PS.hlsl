#include "Fullscreen.hlsli"
#include "Sky.hlsli"
#include "Water.hlsli"

cbuffer WaterParam : register(b0) {
    row_major float4x4 gInvViewProj;
    float32_t3 gCameraPosition;
    float32_t gTime;
    float32_t3 gSunDirection;
    float32_t pad;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t3 kWaterDeepColor = float32_t3(0.01f, 0.06f, 0.09f); // 深い青緑(リニア)
static const float32_t kFresnelF0 = 0.02f;   // 真上から見たときの反射率(水は約 2%)
static const float32_t kFogDensity = 0.012f; // 霞の濃さ(距離に対する指数)
static const float32_t kGlitterPower = 256.0f;
static const float32_t kGlitterIntensity = 3.0f;

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

    float32_t3 normal = WaterNormal(hitXZ, t, gTime);
    float32_t3 viewDir = -dir;

    // フレネル(Schlick): 視線が浅いほど反射が強い
    float32_t nDotV = saturate(dot(normal, viewDir));
    float32_t fresnel = kFresnelF0 + (1.0f - kFresnelF0) * pow(1.0f - nDotV, 5.0f);

    // 反射方向の空の色。水面より下を向く反射は空として無意味なので上向きに折り返す
    float32_t3 reflectDir = reflect(dir, normal);
    reflectDir.y = abs(reflectDir.y);
    float32_t3 reflection = SkyColor(reflectDir, sunDir);

    // 太陽の照り返し(波の揺れできらめく)
    float32_t glitter = pow(saturate(dot(reflectDir, sunDir)), kGlitterPower);

    float32_t3 color = lerp(kWaterDeepColor, reflection, fresnel);
    color += kSunColor * glitter * kGlitterIntensity;

    // 遠方は地平線の空の色へ霞でなじませる(地平線で空と連続する)
    float32_t3 horizonDir = normalize(float32_t3(dir.x, 0.0f, dir.z));
    float32_t fog = 1.0f - exp(-t * kFogDensity);
    color = lerp(color, SkyColor(horizonDir, sunDir), fog);

    output.color = float32_t4(color, 1.0f);
    return output;
}
