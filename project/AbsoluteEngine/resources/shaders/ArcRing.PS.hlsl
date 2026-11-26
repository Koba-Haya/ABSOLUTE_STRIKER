// 切れ目のある輪(ArcRing)のピクセルシェーダー。板の上の座標から、輪の形を計算して描く(テクスチャは使わない)。
//
// 形の決め方:
//   円周を arcCount 個の区間に等分し、各区間の両端 gapRatio / 2 ずつを切れ目にする。弧の端は、tipSoftness で細く消える。
//   輪の幅方向は、innerRadius から outerRadius の間を、edgeSoftness のぼかしで描く。
//   色は、幅の中央(芯)が coreColor、縁が edgeColor。輪の外側と内側には、haloWidth の幅で、光がにじむ。
// 出力は、通常のアルファ合成で使う(明るい空や雲の上でも、色が白く飛ばない)。

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// ArcRing.h の ParamsCB と並びを合わせること
struct ArcRingParams {
    float4 edgeColor;
    float4 coreColor;
    float innerRadius;
    float outerRadius;
    float arcCount;
    float gapRatio;
    float rotation;
    float tipSoftness;
    float edgeSoftness;
    float haloStrength;
    float haloWidth;
    float corePower;
    float intensity;
    float alpha;
};
ConstantBuffer<ArcRingParams> gParams : register(b0);

static const float kTwoPi = 6.28318530718f;
// これより小さい切れ目は、切れ目なし(完全な輪)として扱う
static const float kNoGap = 0.001f;
// 板の縁(半径 1)で、にじむ光が必ず 0 になるよう、この半径から消し始める
static const float kHaloFadeStart = 0.88f;

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    const float2 p = input.texcoord;
    const float r = length(p);

    // --- 弧の形(円周方向) ---
    // 角度を、区間の中の位置(0 〜 1)にする。区間の両端が切れ目
    const float angle = atan2(p.y, p.x) + gParams.rotation;
    const float segment = kTwoPi / max(gParams.arcCount, 1.0f);
    const float a = frac(angle / segment);
    const float gapHalf = gParams.gapRatio * 0.5f;
    const float tip = max(gParams.tipSoftness * (1.0f - gParams.gapRatio), 0.0001f);
    float arcMask = smoothstep(gapHalf, gapHalf + tip, a) * (1.0f - smoothstep(1.0f - gapHalf - tip, 1.0f - gapHalf, a));
    arcMask = (gParams.gapRatio > kNoGap) ? arcMask : 1.0f;

    // --- 輪の幅(半径方向) ---
    const float width = max(gParams.outerRadius - gParams.innerRadius, 0.0001f);
    const float t = (r - gParams.innerRadius) / width; // 0 = 内側の縁、1 = 外側の縁
    const float soft = max(gParams.edgeSoftness, 0.0001f);
    const float stroke = smoothstep(0.0f, soft, t) * (1.0f - smoothstep(1.0f - soft, 1.0f, t));

    // --- にじむ光(輪の外側と内側) ---
    const float outside = max(max(gParams.innerRadius - r, r - gParams.outerRadius), 0.0f); // 輪の帯からの距離
    const float halo = exp(-outside / gParams.haloWidth) * gParams.haloStrength * (1.0f - stroke);
    const float haloFade = 1.0f - smoothstep(kHaloFadeStart, 1.0f, r);

    // --- 色: 幅の中央ほど芯の色、縁と光は縁の色 ---
    const float center = 1.0f - abs(2.0f * saturate(t) - 1.0f);
    const float coreWeight = pow(center, gParams.corePower) * stroke;
    const float3 color = lerp(gParams.edgeColor.rgb, gParams.coreColor.rgb, coreWeight) * gParams.intensity;

    const float coverage = saturate(stroke + halo * haloFade) * arcMask;
    output.color = float4(color, coverage * gParams.alpha);
    return output;
}
