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
    float dissolveEmber; // 縁を熾火(灰 -> 赤 -> 橙)で光らせる強さ(0 で従来の「縁の色へ混ぜるだけ」)
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

// ディゾルブの縁を熾火のように光らせる配色(不死鳥が燃え尽きて灰になる見た目)。
// 帯の中の位置 w(0 = いまにも消える外縁、1 = 帯の内側の端)で、次のように色が変わる:
//   0 〜 ASH_END        : 灰色(焼け焦げた縁。外縁ほど灰)
//   ASH_END 〜 GLOW_END : 暗い赤から、縁の色(橙)へ(熾火の光る部分)
//   GLOW_END 〜 1       : 光が引いて、元の色へ戻る
static const float3 EMBER_ASH_COLOR = float3(0.30f, 0.28f, 0.26f);  // 灰の色
static const float3 EMBER_RED_COLOR = float3(0.55f, 0.08f, 0.02f);  // 熾火の暗い赤
static const float EMBER_ASH_END = 0.25f;
static const float EMBER_GLOW_END = 0.55f;
static const float EMBER_INTENSITY = 1.6f;                          // 熾火の明るさ(色に掛ける。出力は 1 で頭打ち)

static float3 ApplyEmber(float3 baseColor, float3 hotColor, float w)
{
    float3 glow = lerp(EMBER_RED_COLOR, hotColor, smoothstep(EMBER_ASH_END, EMBER_GLOW_END, w)) * EMBER_INTENSITY;
    float3 burnt = lerp(EMBER_ASH_COLOR, glow, smoothstep(0.0f, EMBER_ASH_END, w));
    return lerp(burnt, baseColor, smoothstep(EMBER_GLOW_END, 1.0f, w));
}

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
            float3 plainEdge = lerp(finalRGB, gMaterial.dissolveEdgeColor, mixFactor);
            // 熾火の強さが 0 のとき(敵など)は従来どおり。強さに応じて、熾火の配色へ移る
            float3 emberEdge = ApplyEmber(finalRGB, gMaterial.dissolveEdgeColor, edgeWeight);
            finalRGB = lerp(plainEdge, emberEdge, saturate(gMaterial.dissolveEmber));
        }
    }

    output.color = float4(finalRGB, alpha);
    return output;
}