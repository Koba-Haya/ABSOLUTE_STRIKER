// シールドのドーム(ShieldDome)のピクセルシェーダーの本体。
// 2 つのピクセルシェーダーが、これを取り込む:
//   ShieldDome.PS.hlsl         : 深度は、描画先の深度バッファ(深度テスト)で扱う。雲の合成の前に描くとき
//   ShieldDomeSrvDepth.PS.hlsl : SHIELD_USE_SRV_DEPTH を定義して、シーンの深度をテクスチャとして読み、自分で比べる。
//                                雲の合成のあと(描画先が色だけ)に描くとき
//
// 見た目:
//   三角形の格子の線 + 面ごとのゆるい明滅 + 縁の光(フレネル)。通常のアルファ合成で出す(明るい空の上でも、色が白く飛ばない)。
//   耐久が減る(hpRatio が小さくなる)と、面ごとの乱数が hpRatio 以上の面が、傷んだ色(赤)で、途切れ途切れに明滅する。
//   砕ける(頂点シェーダーが面を飛ばす)と、飛びながら薄れて消える。現れる(appear)ときは、乱数の小さい面から現れる。
//   被弾した向きから、波紋が球の表面を広がり、通った面と線が明るくなる。警告(warning)の間は、赤く脈打つ。

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 direction : TEXCOORD1;
    float3 barycentric : TEXCOORD2;
    float faceRandom : TEXCOORD3;
    float shatter : TEXCOORD4;
};

static const uint kMaxHits = 8; // ShieldDome.h の kMaxHits と同じ値にすること

// ShieldDome.h の ParamsCB と並びを合わせること
struct ShieldParams {
    float4 baseColor;
    float4 lineColor;
    float4 damageColor;
    float4 rippleColor;
    float3 cameraPosition;
    float time;
    float hpRatio;
    float breakProgress;
    float appear;
    float warning;
    float alpha;
    float lineWidth;
    float fresnelPower;
    float hitCount;
    float rippleSpeed;
    float rippleWidth;
    float rippleLife;
    float pad;
    float4 hits[kMaxHits]; // xyz = 被弾の向き、w = 経過秒
};
ConstantBuffer<ShieldParams> gParams : register(b0);

#ifdef SHIELD_USE_SRV_DEPTH
Texture2D<float> gSceneDepth : register(t0);
#endif

// 明るさの調整値
static const float kFaceMin = 0.05f;          // 面の明るさ(乱数が最小の面)
static const float kFaceMax = 0.26f;          // 面の明るさ(乱数が最大の面)
static const float kFaceShimmer = 0.35f;      // 面の明滅の深さ
static const float kFaceShimmerSpeed = 1.6f;  // 面の明滅の速さ
static const float kFresnelGain = 1.0f;      // 縁の光の強さ
static const float kLineGain = 1.6f;          // 格子の線の明るさ
static const float kRippleFace = 0.55f;       // 波紋が通った面の明るさ
static const float kRippleGlow = 1.6f;        // 波紋の色(青白い光)の強さ
static const float kRippleAlpha = 0.7f;       // 波紋の濃さ
static const float kShardGlow = 3.0f;         // 破片になった面の線の明るさ(飛び始めに、強く光る)
static const float kRippleLine = 2.2f;        // 波紋が通った線の明るさ(線の明るさへの倍率)
static const float kDamageFlickerSpeed = 7.0f; // 傷んだ面の明滅の速さ
static const float kDamageFlickerSpread = 13.0f; // 傷んだ面ごとに、明滅の位相をずらす量
static const float kDamageDim = 0.35f;        // 傷んだ面の、暗いほうの明るさの割合
static const float kWarningSpeed = 14.0f;     // 警告の脈打つ速さ
static const float kAppearOverlap = 1.15f;    // 現れる範囲の余裕(1 を超えて、最後の面まで確実に現れるようにする)
static const float kDepthBias = 0.0005f;      // シーンの深度との比較の余裕
static const float3 kWarningColor = float3(1.0f, 0.12f, 0.08f);

float4 main(VertexShaderOutput input) : SV_TARGET {
    // --- 格子の線: 三角形の辺からの近さ(重心座標の最小値)を、画面の上での幅で測る。
    // 画面上の変化量(fwidth)は、面を捨てる判定(discard)より前に求める(捨てたピクセルが、隣のピクセルの変化量の計算を乱さないように) ---
    const float edgeDistance = min(input.barycentric.x, min(input.barycentric.y, input.barycentric.z));
    const float lineSoftness = max(fwidth(edgeDistance) * gParams.lineWidth, 0.0001f);
    const float gridLine = 1.0f - smoothstep(0.0f, lineSoftness, edgeDistance);

#ifdef SHIELD_USE_SRV_DEPTH
    // 雲の合成のあと: シーンの深度を読んで、ドームより手前にある物(自機、ボスの体)の後ろは描かない
    const float sceneDepth = gSceneDepth.Load(int3(int2(input.position.xy), 0));
    if (input.position.z > sceneDepth + kDepthBias) {
        discard;
    }
#endif

    const float3 dir = normalize(input.direction);
    const float rnd = input.faceRandom;

    // --- 面の状態: 現れる、砕ける ---
    // 現れる: 乱数が appear の範囲内の面だけが見える。砕ける: 飛びながら、薄れて消える(飛び終えた面は、描かない)
    const float appearing = step(rnd, gParams.appear * kAppearOverlap);
    const float present = appearing * (1.0f - input.shatter);
    if (present <= 0.0f) {
        discard;
    }

    // --- 縁の光(フレネル): 視線に対して浅い角度の所ほど光る ---
    const float3 viewDir = normalize(gParams.cameraPosition - input.worldPos);
    const float fresnel = pow(1.0f - saturate(abs(dot(dir, viewDir))), gParams.fresnelPower);

    // --- 面のゆるい明滅 ---
    const float shimmer = 1.0f - kFaceShimmer * (0.5f + 0.5f * sin(gParams.time * kFaceShimmerSpeed + rnd * 25.0f));
    float face = lerp(kFaceMin, kFaceMax, rnd) * shimmer;

    // --- 傷み: 耐久が減るほど、乱数が hpRatio 以上の面(= 割合が 1 - hpRatio の面)が傷む ---
    const float damaged = step(gParams.hpRatio, rnd);
    const float damageFlicker = lerp(kDamageDim, 1.0f, step(0.5f, frac(gParams.time * kDamageFlickerSpeed + rnd * kDamageFlickerSpread)));

    // --- 被弾の波紋: 被弾の向きとの角度が、波の半径(経過時間 x 速さ)に近い所が光る ---
    float ripple = 0.0f;
    const uint hitCount = (uint)gParams.hitCount;
    for (uint i = 0; i < kMaxHits; ++i) {
        if (i >= hitCount) break;
        const float angle = acos(clamp(dot(dir, gParams.hits[i].xyz), -1.0f, 1.0f));
        const float age = gParams.hits[i].w;
        const float x = (angle - age * gParams.rippleSpeed) / gParams.rippleWidth;
        ripple += exp(-x * x) * saturate(1.0f - age / gParams.rippleLife);
    }

    // --- 色 ---
    float3 faceColor = lerp(gParams.baseColor.rgb, gParams.damageColor.rgb, damaged);
    float3 lineColor = lerp(gParams.lineColor.rgb, gParams.damageColor.rgb, damaged * 0.6f);
    // 警告: 赤く脈打つ
    const float warningPulse = gParams.warning * (0.5f + 0.5f * sin(gParams.time * kWarningSpeed));
    faceColor = lerp(faceColor, kWarningColor, warningPulse);
    lineColor = lerp(lineColor, kWarningColor, warningPulse);

    face *= lerp(1.0f, damageFlicker, damaged);
    const float lineStrength = gridLine * kLineGain * (1.0f + ripple * kRippleLine + input.shatter * kShardGlow) * lerp(1.0f, damageFlicker, damaged);
    const float faceStrength = face + ripple * kRippleFace;

    const float3 rgb = faceColor * (faceStrength + fresnel * kFresnelGain) + lineColor * lineStrength + gParams.rippleColor.rgb * (ripple * kRippleGlow);
    const float coverage = saturate(faceStrength + fresnel * kFresnelGain + lineStrength + ripple * kRippleAlpha);
    return float4(rgb, coverage * gParams.alpha * present);
}
