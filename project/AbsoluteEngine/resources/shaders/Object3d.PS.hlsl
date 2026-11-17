#include "object3d.hlsli"
#include "CrackVoronoi.hlsli"
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

    // --- Crack (ボロノイのひび割れ。撃破演出用。enableCrack が 0 なら何もしない) ---
    int enableCrack;
    float crackCellScale;    // 1 / セルの大きさ(モデル本来の単位)。大きいほど、細かく割れる
    float crackLineWidth;    // ひびの線の太さの最大値(セルの大きさに対する割合)
    float crackReveal;       // ひびが広がった割合(0 = なし、1 = 全体に広がり切った)
    float3 crackColor;       // ひびの光る色
    float crackIntensity;    // ひびの光の強さ(色に掛ける)
    float3 crackOrigin;      // ひびが広がり始める位置(モデル本来の座標)
    float crackSpreadRadius; // origin から、ひびが最後に届く所までの距離
    float crackBreak;        // セルが消えた割合(0 = 消えない、1 = すべて消えた)。消える前のセルは、面全体が光る
    float3 crackPad;
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
// 光る球の殻(ModelLightingMode::kRimGlow)。照明は使わない。
// 視線に斜めの度合い rim(0 = 正対する中心、1 = 縁)を、rim = (1 - 正対の度合い) ^ shininess で求める。
// shininess が大きいほど、縁の細い輪だけが濃くなる(球の中は、ほぼ透ける)。中心の色(specularColor)から縁の色(color)へ変え、
// 透明度も、中心は RIM_GLOW_CENTER_ALPHA 倍、縁は 1 倍にする(中心が透けるので、球の中の物が見える)
static const int LIGHTING_MODE_RIM_GLOW = 4;
static const float RIM_GLOW_CENTER_ALPHA = 0.04f;
// shininess の下限(0 乗は、どこでも 1 になって、縁と中心の差が消えるため)
static const float RIM_GLOW_MIN_SHARPNESS = 0.01f;

// 爆散のプラズマ球(ModelLightingMode::kPlasma)。照明は使わず、視線との向きと、球面の上を流れるノイズで、色を決める。
//   中心 = 白く飛ぶほど明るい白、縁 = 縁の色(color)へ変わる(rim = (1 - 正対の度合い) ^ shininess)
//   球面の斑 = ノイズの高い所を、縁の色へ寄せる(球の中に残った、崩れる体の影のように見える)。斑の量は environmentCoefficient(0 で斑なし)
//   ノイズの流れる位相 = specularColor.x(秒。外から時間を渡す)
static const int LIGHTING_MODE_PLASMA = 5;
// 球面ノイズの細かさ(球の法線に掛ける倍率。大きいほど、斑が細かい)と、流れる速さ(位相に掛ける倍率)
static const float PLASMA_NOISE_SCALE = 2.4f;
static const float3 PLASMA_FLOW_SPEED = float3(0.35f, -0.25f, 0.20f);
// 斑になるノイズの値の範囲(この範囲で、0 から 1 へ)
static const float PLASMA_PATCH_LOW = 0.48f;
static const float PLASMA_PATCH_HIGH = 0.68f;
// 中心の白の明るさ(1 を超えて、出力で頭打ちにして、白く飛ばす)
static const float PLASMA_CORE_BRIGHTNESS = 1.6f;
// 縁の色へ寄る強さ(rim に掛ける。大きいほど、縁の色が、内側まで広がる)
static const float PLASMA_RIM_GAIN = 1.3f;
// 斑の色の明るさ(縁の色に掛ける。1 より小さくして、斑を少し暗くする)
static const float PLASMA_PATCH_DARKEN = 0.85f;
// ふわっとした光の輪(ModelLightingMode::kSoftGlow)。球の中心ほど濃く、輪郭へ向けて、なめらかに透ける。
// 色 = color の rgb、濃さ = color.a * (正対の度合い ^ shininess)。プラズマ球より大きい球に使うと、プラズマ球の外側に、光がにじんで見える
static const int LIGHTING_MODE_SOFT_GLOW = 6;

// 3 次元の値ノイズの、格子点の乱数(0 〜 1)
static float PlasmaHash(float3 p)
{
    p = frac(p * 0.3183099f + 0.1f);
    p *= 17.0f;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3 次元の値ノイズ(0 〜 1)。格子点の乱数を、なめらかに補間する
static float PlasmaNoise(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0f - 2.0f * f);
    float n000 = PlasmaHash(i);
    float n100 = PlasmaHash(i + float3(1, 0, 0));
    float n010 = PlasmaHash(i + float3(0, 1, 0));
    float n110 = PlasmaHash(i + float3(1, 1, 0));
    float n001 = PlasmaHash(i + float3(0, 0, 1));
    float n101 = PlasmaHash(i + float3(1, 0, 1));
    float n011 = PlasmaHash(i + float3(0, 1, 1));
    float n111 = PlasmaHash(i + float3(1, 1, 1));
    return lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y),
                lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
}

// 3 段重ねたノイズ(大きな斑に、細かい斑を重ねる。0 〜 約 1)
static float PlasmaFbm(float3 p)
{
    return PlasmaNoise(p) * 0.55f + PlasmaNoise(p * 2.03f) * 0.30f + PlasmaNoise(p * 4.11f) * 0.15f;
}

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

// ===== ボロノイのひび割れ =====
// ひびが先頭から広がる帯の幅(広がり具合に対する割合。大きいほど、先端がぼやけて、ゆっくり太くなる)
static const float CRACK_SPREAD_SOFTNESS = 0.35f;
// ひびの周りに、にじむ光の幅(線の太さに対する倍率)と、その強さ
static const float CRACK_HALO_SCALE = 3.0f;
static const float CRACK_HALO_STRENGTH = 0.35f;

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

    // 光る球の殻: 照明の計算はせず、視線との向きだけで、色と透明度を決める
    if (gMaterial.enableLighting == LIGHTING_MODE_RIM_GLOW)
    {
        float3 glowNormal = normalize(input.normal);
        float3 glowView = normalize(gCamera.worldPosition - input.worldPosition);
        float facing = abs(dot(glowNormal, glowView)); // 裏面も、表面と同じ扱い
        float rim = pow(1.0f - facing, max(gMaterial.shininess, RIM_GLOW_MIN_SHARPNESS));
        float3 glowRGB = lerp(gMaterial.specularColor, gMaterial.color.rgb, rim);
        float glowAlpha = gMaterial.color.a * lerp(RIM_GLOW_CENTER_ALPHA, 1.0f, rim);
        output.color = float4(glowRGB, glowAlpha);
        return output;
    }

    // 爆散のプラズマ球: 中心は白く飛び、縁は縁の色。球面を流れる斑が、縁の色へ寄る
    if (gMaterial.enableLighting == LIGHTING_MODE_PLASMA)
    {
        float3 plasmaNormal = normalize(input.normal);
        float3 plasmaView = normalize(gCamera.worldPosition - input.worldPosition);
        float plasmaFacing = abs(dot(plasmaNormal, plasmaView));
        float plasmaRim = saturate(pow(1.0f - plasmaFacing, max(gMaterial.shininess, RIM_GLOW_MIN_SHARPNESS)) * PLASMA_RIM_GAIN);
        float3 plasmaQ = plasmaNormal * PLASMA_NOISE_SCALE + PLASMA_FLOW_SPEED * gMaterial.specularColor.x;
        float plasmaPatch = smoothstep(PLASMA_PATCH_LOW, PLASMA_PATCH_HIGH, PlasmaFbm(plasmaQ)) * saturate(gMaterial.environmentCoefficient);
        float3 plasmaRGB = lerp(float3(1.0f, 1.0f, 1.0f) * PLASMA_CORE_BRIGHTNESS, gMaterial.color.rgb, plasmaRim);
        plasmaRGB = lerp(plasmaRGB, gMaterial.color.rgb * PLASMA_PATCH_DARKEN, plasmaPatch);
        output.color = float4(plasmaRGB, gMaterial.color.a);
        return output;
    }

    // ふわっとした光の輪: 中心ほど濃く、輪郭へ向けて透ける
    if (gMaterial.enableLighting == LIGHTING_MODE_SOFT_GLOW)
    {
        float3 softNormal = normalize(input.normal);
        float3 softView = normalize(gCamera.worldPosition - input.worldPosition);
        float softFacing = abs(dot(softNormal, softView));
        float softGlow = pow(softFacing, max(gMaterial.shininess, RIM_GLOW_MIN_SHARPNESS));
        output.color = float4(gMaterial.color.rgb, gMaterial.color.a * softGlow);
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

    // ===== ボロノイのひび割れ =====
    // 照明の計算のあとに、色を直接のせる(ひびは、自分で光るので、影の側でも暗くならない)
    if (gMaterial.enableCrack != 0)
    {
        // localPosition は、モデル本来の頂点位置(スキニング前)。表面の同じ点は、動いても同じ値なので、ひびは、ボスに付いてくる
        float3 nearestFeature;
        float3 nearestId;
        float edge = CrackEdgeDistance(input.localPosition * gMaterial.crackCellScale, nearestFeature, nearestId);

        // セルごとの光り始める時刻(計算は、粒を出す側と共有する CrackVoronoi.hlsli)。
        // 代表点と番号は、セルの中で同じ値なので、1 つのセルは、まとめて光って、まとめて消える
        float heatStart = CrackCellHeatStart(nearestFeature, nearestId, gMaterial.crackCellScale, gMaterial.crackOrigin, gMaterial.crackSpreadRadius);
        // crackBreak が heatStart を越えると、セルの面が光りだし、HEAT_WINDOW のあとに消える(crackBreak = 1 で、必ず全部消える)
        if (gMaterial.crackBreak >= heatStart + CRACK_BREAK_HEAT_WINDOW)
        {
            discard;
        }
        float cellHeat = saturate((gMaterial.crackBreak - heatStart) / CRACK_BREAK_HEAT_WINDOW);

        // 広がり: origin に近い所から先に太くなる。dist01 は、origin(0)から、いちばん遠い所(1)までの割合
        float dist01 = length(input.localPosition - gMaterial.crackOrigin) / max(gMaterial.crackSpreadRadius, CRACK_MIN_VALUE);
        float front = gMaterial.crackReveal * (1.0f + CRACK_SPREAD_SOFTNESS);
        float spread = saturate((front - dist01) / CRACK_SPREAD_SOFTNESS);

        float width = gMaterial.crackLineWidth * spread;
        float crackLine = (1.0f - smoothstep(0.0f, max(width, CRACK_MIN_VALUE), edge)) * step(CRACK_MIN_VALUE, width);
        float crackHalo = (1.0f - smoothstep(0.0f, max(width * CRACK_HALO_SCALE, CRACK_MIN_VALUE), edge)) * step(CRACK_MIN_VALUE, width);

        float3 crackRGB = gMaterial.crackColor * gMaterial.crackIntensity;
        // 消える直前のセルは、面全体が、ひびと同じ光になる
        finalRGB = lerp(finalRGB, crackRGB, cellHeat);
        finalRGB = lerp(finalRGB, crackRGB, saturate(crackLine));
        finalRGB += crackRGB * crackHalo * CRACK_HALO_STRENGTH;
    }

    output.color = float4(finalRGB, alpha);
    return output;
}