// 火の玉(EnergyOrb)のピクセルシェーダー。板の上の座標から、球の陰影つきの火の玉を計算して描く(テクスチャは使わない)。
//
// 立体に見せる仕組み: 板の上の点を「球の表面の点」とみなし、法線(球の面の向き)を求める(z = sqrt(1 - x^2 - y^2))。
// 法線から、縁の暗さ(フレネル)、鏡面のハイライト(光源は、左上の手前)、表面の 3D ノイズの座標を決める。
// 板は常にカメラを向くが、陰影は球のもの(中心が明るく、縁が暗く、ハイライトが片側に寄る)。
// 自転はさせない: 表面の模様は、3D ノイズの座標を、流れの向き(flow)へ動かして作る。球ごと回らず、模様が流れて沸く。
//
// 見た目(下の層から、上の層へ、アルファ合成で重ねる):
//   外側の暗い輪郭 -> 外へにじむ紫の光 -> 球の外へ流れる炎(ノイズ)-> 球(黒紫の本体、熱い所は紫から白へ)-> 球から離れて消える火花
// 溜まり具合(charge)が増えると、球が熱くなり、炎が遠くへ伸び、球が震える。
// 出力は、通常のアルファ合成(色は、アルファで割った値)。

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// EnergyOrb.h の ParamsCB と並びを合わせること
struct EnergyOrbParams {
    float4 bodyColor;
    float4 glowColor;
    float4 coreColor;
    float4 outlineColor;
    float time;
    float seed;
    float appear;
    float charge;
    float orbRadius;
    float haloStrength;
    float intensity;
    float alpha;
    float2 flow;
    float stretch;
    float padding;
};
ConstantBuffer<EnergyOrbParams> gParams : register(b0);

static const float kTwoPi = 6.28318530718f;
// 板の縁(半径 1)で、すべての光が必ず 0 になるよう、この半径から消し始める
static const float kEdgeFadeStart = 0.80f;
static const float kEdgeFadeEnd = 0.98f;
// 現れる前の、玉の大きさの割合(現れるにつれて 1 へ)
static const float kAppearStartScale = 0.30f;
// 溜まったときの震え(玉の半径の振れ幅の割合と、速さ)
static const float kShakeAmount = 0.07f;
static const float kShakeSpeed = 38.0f;
// 球の表面の模様: ノイズの細かさ、流れる速さ、沸く速さ、ゆがみの強さ
static const float kSurfaceNoiseScale = 3.0f;
static const float kSurfaceFlowSpeed = 0.7f;
static const float kSurfaceBoilSpeed = 0.35f;
static const float kSurfaceWarp = 0.55f;
// 球の陰影: 縁の暗さ(フレネルの指数と、暗くする強さ)、縁の薄い光の強さ
static const float kFresnelPower = 2.2f;
static const float kRimDarken = 0.80f;
static const float kRimGlowStrength = 0.70f;
// 鏡面のハイライト(光源の向きは、左上の手前)。火の玉なので、つやは弱くする
static const float3 kLightDirection = float3(-0.35f, 0.55f, 0.75f);
static const float kSpecularPower = 40.0f;
static const float kSpecularStrength = 0.45f;
// 球の熱さ(0 = 暗い、1 = 白): 正面ほど熱く、ノイズで、熱い所と暗い所に分かれる(溶岩のような模様)。溜まると、熱くなる
static const float kHotNoiseGain = 1.70f;
static const float kHotNoiseOffset = 0.50f;
static const float kHotChargeBoost = 0.25f;
// 炎: ノイズの細かさ、流れる速さ、出る・消えるしきい値と、外へ消える強さ、濃さの倍率、溜まったときに伸びる割合
static const float kFlameNoiseScale = 1.3f;
static const float kFlameFlowSpeed = 1.2f;
static const float kFlameThreshold = 0.15f;
static const float kFlameFalloff = 0.60f;
static const float kFlameGain = 3.5f;
static const float kFlameReachCharge = 0.50f;
// 外側の暗い輪郭と、にじむ紫の光の、広がり(球の半径を 1 とした距離)と濃さ
static const float kAuraSpread = 0.55f;
static const float kAuraAlpha = 0.55f;
static const float kHaloSpread = 0.90f;
static const float kHaloAlpha = 0.55f;
// 火花: 数、出て消えるまでの速さ、板の上の大きさ(最小と振れ幅)
static const int kSparkCount = 6;
static const float kSparkRate = 0.9f;
static const float kSparkSizeMin = 0.010f;
static const float kSparkSizeRange = 0.008f;

float Hash11(float n) {
    return frac(sin(n * 127.1f) * 43758.5453f);
}

float Hash31(float3 p) {
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

// 3D の値ノイズ(格子点の乱数を、なめらかに補間する)
float Noise3(float3 p) {
    const float3 i = floor(p);
    const float3 f = frac(p);
    const float3 u = f * f * (3.0f - 2.0f * f);
    const float n000 = Hash31(i);
    const float n100 = Hash31(i + float3(1.0f, 0.0f, 0.0f));
    const float n010 = Hash31(i + float3(0.0f, 1.0f, 0.0f));
    const float n110 = Hash31(i + float3(1.0f, 1.0f, 0.0f));
    const float n001 = Hash31(i + float3(0.0f, 0.0f, 1.0f));
    const float n101 = Hash31(i + float3(1.0f, 0.0f, 1.0f));
    const float n011 = Hash31(i + float3(0.0f, 1.0f, 1.0f));
    const float n111 = Hash31(i + float3(1.0f, 1.0f, 1.0f));
    return lerp(lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y),
                lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
}

// 3D ノイズを 3 段重ねた、細かさの違う模様
float Fbm3(float3 p) {
    float value = 0.0f;
    float amplitude = 0.5f;
    for (int i = 0; i < 3; ++i) {
        value += amplitude * Noise3(p);
        p *= 2.03f;
        amplitude *= 0.5f;
    }
    return value;
}

// 下の層(bottom)の上に、上の層(top)を重ねる(アルファ合成。色はアルファで割った値で持つ)
float4 Over(float4 top, float4 bottom) {
    const float a = top.a + bottom.a * (1.0f - top.a);
    const float3 c = (top.rgb * top.a + bottom.rgb * bottom.a * (1.0f - top.a)) / max(a, 0.0001f);
    return float4(c, a);
}

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    const float2 p = input.texcoord;
    const float r = length(p);
    const float appear = gParams.appear;
    if (appear <= 0.0f) return output;

    const float charge = gParams.charge;
    const float time = gParams.time + gParams.seed * 10.0f;

    // 流れの向き(板の上。長さ 0 のときは、上向き)
    const float flowLength = length(gParams.flow);
    const float2 flowDir = (flowLength > 0.0001f) ? gParams.flow / flowLength : float2(0.0f, 1.0f);

    // --- 球の大きさ: 現れるにつれて大きくなり、溜まると震える ---
    const float appearEased = 1.0f - (1.0f - appear) * (1.0f - appear);
    const float shake = 1.0f + kShakeAmount * charge * sin(time * kShakeSpeed);
    const float radius = gParams.orbRadius * lerp(kAppearStartScale, 1.0f, appearEased) * shake;
    // 球の半径を 1 とした、板の上の座標。流れの向きの軸に沿って、引き伸ばす(軸方向の座標を 1 / (1 + stretch) 倍すると、軸方向に長い楕円になる)。
    // 飛んでいる玉が、進む向きへ伸びた、彗星のような形になる。stretch が 0 なら、真円のまま
    const float2 scaled = p / radius;
    const float along = dot(scaled, flowDir);
    const float2 d = (scaled - flowDir * along) + flowDir * (along / (1.0f + gParams.stretch));
    const float rd = length(d);        // 球の半径を 1 とした、中心からの距離
    const float outside = max(rd - 1.0f, 0.0f);
    const float edgeFade = 1.0f - smoothstep(kEdgeFadeStart, kEdgeFadeEnd, r);

    const float3 glow = gParams.glowColor.rgb * gParams.intensity * (1.0f + 0.6f * charge);
    const float3 core = gParams.coreColor.rgb * gParams.intensity;

    float4 col = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // 1. 外側の暗い輪郭(明るい空の上で、形が見えるようにする)
    col = Over(float4(gParams.outlineColor.rgb, exp(-outside / kAuraSpread) * kAuraAlpha * edgeFade), col);

    // 2. 外へにじむ紫の光
    col = Over(float4(glow, exp(-outside / kHaloSpread) * gParams.haloStrength * kHaloAlpha * edgeFade), col);

    // 3. 炎: 球の外へ、ノイズの模様が流れる。球から離れるほど消え、溜まるほど遠くへ伸びる
    {
        const float3 flameCoord = float3(d * kFlameNoiseScale - flowDir * time * kFlameFlowSpeed, time * 0.45f + gParams.seed * 3.0f);
        const float flameNoise = Fbm3(flameCoord);
        const float reach = 1.0f + kFlameReachCharge * charge;
        const float flameMask = saturate((flameNoise - kFlameThreshold - outside * kFlameFalloff / reach) * kFlameGain)
                              * smoothstep(0.80f, 1.02f, rd) * (1.0f - smoothstep(1.7f, 2.4f, rd));
        const float3 flameColor = lerp(glow, core, saturate(1.0f - outside * 1.6f) * 0.5f);
        col = Over(float4(flameColor, flameMask * edgeFade), col);
    }

    // 4. 球: 板の上の点を、球の表面の点とみなして、陰影をつける
    {
        const float dd = dot(d, d);
        const float z = sqrt(saturate(1.0f - dd));
        const float3 n = float3(d, z);

        // 表面の 3D ノイズ: 座標を、流れの向きへ動かす(自転ではなく、模様が流れる)。沸くように、時間でもゆがめる
        float3 q = n * kSurfaceNoiseScale - float3(flowDir, 0.0f) * time * kSurfaceFlowSpeed + gParams.seed * 5.0f;
        q += kSurfaceWarp * (Noise3(q * 1.7f + time * kSurfaceBoilSpeed) - 0.5f);
        const float heat = Fbm3(q);

        // 熱さ: 正面(z が大きい)ほど熱く、ノイズで揺らぐ。熱い所ほど、黒紫 -> 紫 -> 白に近い色
        const float hot = saturate(pow(z, 1.1f) * (heat * kHotNoiseGain - kHotNoiseOffset) + kHotChargeBoost * charge);
        float3 surface = lerp(gParams.bodyColor.rgb, glow * 0.75f, smoothstep(0.12f, 0.55f, hot));
        surface = lerp(surface, core, smoothstep(0.62f, 0.98f, hot));

        // 縁(球が横を向く所)は暗くして、丸みを出す。縁には、薄い紫の光を足す
        const float fresnel = pow(1.0f - z, kFresnelPower);
        surface = lerp(surface, gParams.bodyColor.rgb * 0.6f, fresnel * kRimDarken);
        surface += glow * pow(fresnel, 3.0f) * kRimGlowStrength;

        // 鏡面のハイライト(光源の側に寄った、小さな白い光)。陰影が片側に寄るので、平らな円盤に見えない
        const float3 halfVector = normalize(normalize(kLightDirection) + float3(0.0f, 0.0f, 1.0f));
        surface += core * pow(saturate(dot(n, halfVector)), kSpecularPower) * kSpecularStrength;

        const float sphereMask = 1.0f - smoothstep(0.93f, 1.0f, rd);
        col = Over(float4(saturate(surface), sphereMask * 0.98f), col);
    }

    // 5. 火花: 球の表面から、流れの向きへ偏って飛び出し、離れながら消える
    for (int i = 0; i < kSparkCount; ++i) {
        const float h1 = Hash11(float(i) + 1.0f + gParams.seed * 13.0f);
        const float h2 = Hash11(float(i) * 3.7f + 2.0f);
        const float cycle = time * kSparkRate * (0.6f + 0.8f * h2) + h1 * 5.0f;
        const float phase = frac(cycle);
        const float angle = Hash11(floor(cycle) * 7.3f + float(i) * 13.1f + gParams.seed) * kTwoPi;
        const float2 direction = normalize(float2(cos(angle), sin(angle)) + flowDir * 0.8f);
        const float2 sparkPos = direction * radius * (1.0f + 1.1f * phase);
        const float2 delta = p - sparkPos;
        const float size = kSparkSizeMin + kSparkSizeRange * h2;
        const float spark = exp(-dot(delta, delta) / (size * size)) * (1.0f - phase) * smoothstep(0.0f, 0.1f, phase);
        col = Over(float4(core, spark * (0.55f + 0.45f * charge) * edgeFade), col);
    }

    // 現れる前は、薄い(現れる前半で、濃さが 0 から 1 へ)
    const float appearAlpha = smoothstep(0.0f, 0.5f, appear);
    output.color = float4(saturate(col.rgb), col.a * gParams.alpha * appearAlpha);
    return output;
}
