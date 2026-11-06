#include "GPUParticle.hlsli"
#include "GPUParticleGlow.hlsli"

// 発光パスのピクセルシェーダー: やわらかい光の玉(芯、光輪、外光の 3 層のガウス状の減衰)を、熱さの色で塗る。
// 出力は加算で足される(ブレンド: ONE, ONE)ので、色にあらかじめ明るさを掛けておく。
// 雲の合成のあとに描くため、深度は SRV(シーンの深度)を自分で読んで、島などの後ろの粒を隠す。

Texture2D<float> gSceneDepth : register(t2);
ConstantBuffer<GlowParams> gGlow : register(b1);

// 光の層の重み
static const float32_t kCoreWeight = 1.4f;
static const float32_t kHaloWeight = 0.9f;
static const float32_t kOuterWeight = 0.35f;
// 熱さの色の傾き: この熱さで橙になる(それより低いと赤 -> 橙、高いと橙 -> 黄)
static const float32_t kMidHeat = 0.55f;
// この熱さ以下は見えなくなる(冷めきった灰は、発光しない)
static const float32_t kHeatFloor = 0.08f;
// 芯が白く光り始める熱さ
static const float32_t kCoreHeatMin = 0.5f;
// 板の縁で光を 0 にし始める半径(板の端で切れて見えないように)
static const float32_t kEdgeStart = 0.6f;
// 粒自身の色で光らせるとき: 芯に混ぜる粒の色の割合(1 で芯も粒の色、0 で白に近い芯)
static const float32_t kOwnColorCoreMix = 0.6f;
static const float32_t kOwnColorThreshold = 0.5f;

struct PixelShaderInput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD;
    float32_t heat : HEAT;
    float32_t brightness : BRIGHTNESS;
    float32_t3 tint : TINT;
    float32_t ownColor : OWNCOLOR;
};

float32_t4 main(PixelShaderInput input) : SV_TARGET0 {
    // 深度テスト(雲のあとは描画先に深度が無いので、自分で比較する)
    float32_t sceneDepth = gSceneDepth.Load(int32_t3(int32_t2(input.position.xy), 0));
    if (input.position.z > sceneDepth + gGlow.depthBias) {
        discard;
    }

    float32_t2 p = (input.texcoord - 0.5f) * 2.0f;
    float32_t r2 = dot(p, p);
    if (r2 >= 1.0f) {
        discard;
    }
    float32_t edge = 1.0f - smoothstep(kEdgeStart, 1.0f, r2);

    float32_t core = exp(-r2 * gGlow.coreSharpness);
    float32_t halo = exp(-r2 * gGlow.haloSharpness);
    float32_t outer = exp(-r2 * gGlow.outerSharpness);

    // 熱さで色を決める(低温は暗い赤、中温は橙、高温は黄)。芯は熱いほど白に近い黄になる
    float32_t heat = input.heat;
    float32_t3 hotColor = heat < kMidHeat
        ? lerp(gGlow.colorRed, gGlow.colorOrange, heat / kMidHeat)
        : lerp(gGlow.colorOrange, gGlow.colorYellow, (heat - kMidHeat) / (1.0f - kMidHeat));
    float32_t visible = smoothstep(0.0f, kHeatFloor, heat);
    float32_t3 coreBase = gGlow.colorCore;
    float32_t coreGate = smoothstep(kCoreHeatMin, 1.0f, heat);
    if (input.ownColor > kOwnColorThreshold) {
        // 粒自身の色(例: 金から青緑へ冷める鱗粉)。熱さに関係なく見える(消えるのはアルファが下がるとき)
        hotColor = input.tint;
        visible = 1.0f;
        coreBase = lerp(gGlow.colorCore, input.tint, kOwnColorCoreMix);
        coreGate = 1.0f;
    }

    float32_t3 body = hotColor * (halo * kHaloWeight + outer * kOuterWeight);
    float32_t3 coreColor = coreBase * core * kCoreWeight * coreGate;
    float32_t3 rgb = (body + coreColor) * visible * edge * input.brightness;
    return float32_t4(rgb, 1.0f);
}
