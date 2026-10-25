#include "object3d.hlsli"
#define MAX_POINT_LIGHTS 16
#define MAX_DIR_LIGHTS   4
#define MAX_SPOT_LIGHTS  8

struct Material
{
    float4 color;
    int enableLighting;
    float3 specularColor;
    float4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    float2 pad;
    
    // --- Dissolve ---
    int enableDissolve;
    float dissolveThreshold;
    float dissolveEdgeRange;
    float pad2;
    float3 dissolveEdgeColor;
    float pad3;
    float3 dissolveMaskColor;
    float pad4;
};

struct Camera
{
    float3 worldPosition;
    float pad;
};

struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き
    float intensity; // 輝度
    int enabled; // 0:OFF 1:ON
    float3 pad0; // 16byte境界
};

struct DirectionalLightGroup
{
    int count;
    float3 padCount;
    DirectionalLight lights[MAX_DIR_LIGHTS];
    int enabled;
    float3 padEnabled; // 16byte境界
};

struct PointLight
{
    float4 color; // ライトの色
    float3 position; // ライトの位置（コメント修正しておくと良い）
    float intensity; // 輝度
    float radius; // 光の届く最大距離
    float decay; // 減衰率
    int enabled; // 0:OFF 1:ON
    float pad0; // 16byte境界
};

struct PointLightGroup
{
    int count;
    float3 padCount; // 16byte境界
    PointLight lights[MAX_POINT_LIGHTS];
    int enabled; // 0:OFF 1:ON
};

struct SpotLight
{
    float4 color; // ライトの色
    float3 position; // ライトの位置
    float intensity; // 輝度
    float3 direction; // ライトの向き
    float distance; // 光の届く最大距離
    float decay; // 減衰率
    float cosAngle; // 内側の角度
    int enabled; // 0:OFF 1:ON
    float pad0; // 16byte境界
};

struct SpotLightGroup
{
    int count;
    float3 padCount;
    SpotLight lights[MAX_SPOT_LIGHTS];
    int enabled;
    float3 padEnabled; // 16byte境界
};

ConstantBuffer<Material> gMaterial : register(b0); // マテリアル定数バッファ
Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1); // 環境マップ（キューブマップ）
Texture2D<float> gMaskTexture : register(t2); // Dissolve用
SamplerState gSampler : register(s0); // テクスチャサンプラー
ConstantBuffer<DirectionalLightGroup> gDirectionalLights : register(b1); // 複数の平行光をまとめた定数バッファ
ConstantBuffer<Camera> gCamera : register(b2); // カメラの定数バッファ
ConstantBuffer<PointLightGroup> gPointLights : register(b3); // 複数の点光源をまとめた定数バッファ
ConstantBuffer<SpotLightGroup> gSpotLights : register(b4); // 複数のスポットライトをまとめた定数バッファ

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 照明モード(Material の enableLighting)。0 = 照明なし、1 = Lambert、それ以外 = Half-Lambert。
// 3 = 薄い面(蝶の翅など)。Half-Lambert に、光の当たらない側でも真っ黒にならないよう、環境光を足す
static const int LIGHTING_MODE_THIN_SURFACE = 3;
// 薄い面の環境光の強さ(面の色に掛ける)。光が当たらない側(翅の裏、影の側)の最低限の明るさ
static const float THIN_SURFACE_AMBIENT = 0.45f;

static float3 ComputeDiffuse(float3 N, float3 Ldir, int lightingMode)
{
    float ndotl = dot(N, Ldir);

    if (lightingMode == 1)  // Lambert
    {
        return saturate(ndotl);
    }
    else // Half-Lambert
    {
        return pow(saturate(ndotl * 0.5f + 0.5f), 2.0f);
    }
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (textureColor.a <= 0.5f)
        discard;

    float alpha = gMaterial.color.a * textureColor.a;
    if (alpha <= 0.0f)
        discard;

    if (gMaterial.enableLighting == 0)
    {
        output.color = float4(gMaterial.color.rgb * textureColor.rgb, alpha);
        return output;
    }

    float dirGroupOn = (gDirectionalLights.enabled != 0) ? 1.0f : 0.0f;
    float pointGroupOn = (gPointLights.enabled != 0) ? 1.0f : 0.0f;
    float spotGroupOn = (gSpotLights.enabled != 0) ? 1.0f : 0.0f;

    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    float3 diffuseSum = float3(0.0f, 0.0f, 0.0f);
    float3 specularSum = float3(0.0f, 0.0f, 0.0f);

    // ===== Directional Lights (multiple) =====
    int dCount = min(gDirectionalLights.count, MAX_DIR_LIGHTS);

    [loop]
    for (int i = 0; i < dCount; ++i)
    {
        DirectionalLight dl = gDirectionalLights.lights[i];
        if (dl.enabled == 0)
            continue;

        float3 Ld = normalize(-dl.direction); // 面→光
        float diff = (gMaterial.enableLighting == 1)
            ? saturate(dot(N, Ld))
            : pow(saturate(dot(N, Ld) * 0.5f + 0.5f), 2.0f);

        diffuseSum +=
            gMaterial.color.rgb * textureColor.rgb *
            diff *
            dl.color.rgb * dl.intensity * dirGroupOn;

        if (gMaterial.shininess > 0.0f)
        {
            float3 H = normalize(Ld + V);
            float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);

            specularSum +=
                dl.color.rgb * dl.intensity *
                gMaterial.specularColor *
                specPow * dirGroupOn;
        }
    }

    // ===== Point Lights (multiple) =====
    int pCount = min(gPointLights.count, MAX_POINT_LIGHTS);

    [loop]
    for (int i = 0; i < pCount; ++i)
    {
        PointLight pl = gPointLights.lights[i];
        if (pl.enabled == 0)
            continue;

        float3 L = pl.position - input.worldPosition; // 面→点光源
        float dist = length(L);
        float3 Ldir = (dist > 0.0001f) ? (L / dist) : float3(0.0f, 1.0f, 0.0f);

        float attenuation = pow(saturate(-dist / pl.radius + 1.0f), pl.decay);

        float ndotl = dot(N, Ldir);
        float diff = (gMaterial.enableLighting == 1)
            ? saturate(ndotl)
            : pow(saturate(ndotl * 0.5f + 0.5f), 2.0f);

        diffuseSum +=
            gMaterial.color.rgb * textureColor.rgb *
            diff *
            pl.color.rgb * pl.intensity *
            attenuation * pointGroupOn;

        if (gMaterial.shininess > 0.0f)
        {
            float3 H = normalize(Ldir + V);
            float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);

            specularSum +=
                pl.color.rgb * pl.intensity *
                gMaterial.specularColor *
                specPow *
                attenuation * pointGroupOn;
        }
    }

    // ===== Spot Lights (multiple) =====
    int sCount = min(gSpotLights.count, MAX_SPOT_LIGHTS);

    [loop]
    for (int i = 0; i < sCount; ++i)
    {
        SpotLight sl = gSpotLights.lights[i];
        if (sl.enabled == 0)
            continue;

        float3 Lvec = sl.position - input.worldPosition; // 面→ライト
        float dist = length(Lvec);
        float3 Ldir = (dist > 0.0001f) ? (Lvec / dist) : float3(0.0f, 1.0f, 0.0f);

        float attenuation = pow(saturate(-dist / sl.distance + 1.0f), sl.decay);

        float3 lightToSurfaceDir = normalize(input.worldPosition - sl.position); // ライト→面
        float cosTheta = dot(lightToSurfaceDir, normalize(sl.direction));

        float falloff = saturate((cosTheta - sl.cosAngle) / (1.0f - sl.cosAngle));

        float ndotl = dot(N, Ldir);
        float diff = (gMaterial.enableLighting == 1)
            ? saturate(ndotl)
            : pow(saturate(ndotl * 0.5f + 0.5f), 2.0f);

        diffuseSum +=
            gMaterial.color.rgb * textureColor.rgb *
            diff *
            sl.color.rgb * sl.intensity *
            attenuation * falloff * spotGroupOn;

        if (gMaterial.shininess > 0.0f)
        {
            float3 H = normalize(Ldir + V);
            float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);

            specularSum +=
                sl.color.rgb * sl.intensity *
                gMaterial.specularColor *
                specPow *
                attenuation * falloff * spotGroupOn;
        }
    }

    float3 finalRGB = diffuseSum + specularSum;

    // 薄い面は、光の当たらない側も、環境光でうっすら見えるようにする
    if (gMaterial.enableLighting == LIGHTING_MODE_THIN_SURFACE)
    {
        finalRGB += gMaterial.color.rgb * textureColor.rgb * THIN_SURFACE_AMBIENT;
    }
    
    // ===== 環境マッピング (反射) =====
    if (gMaterial.enableLighting != 0 && gMaterial.environmentCoefficient > 0.0f)
    {
        float3 N = normalize(input.normal);
        float3 incident = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(incident, N);
        float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector).rgb;
        finalRGB += environmentColor * gMaterial.environmentCoefficient;
    }

    // ===== Dissolve =====
    if (gMaterial.enableDissolve != 0)
    {
        float mask = gMaskTexture.Sample(gSampler, transformedUV.xy).r;
        
        if (mask <= gMaterial.dissolveThreshold)
        {
            discard;
        }
        else if (mask <= gMaterial.dissolveThreshold + gMaterial.dissolveEdgeRange)
        {
            float edgeWeight = (mask - gMaterial.dissolveThreshold) / gMaterial.dissolveEdgeRange;
            float mixFactor = 1.0f - edgeWeight;
            finalRGB = lerp(finalRGB, gMaterial.dissolveEdgeColor, mixFactor);
        }
    }

    output.color = float4(finalRGB, alpha);
    return output;
}