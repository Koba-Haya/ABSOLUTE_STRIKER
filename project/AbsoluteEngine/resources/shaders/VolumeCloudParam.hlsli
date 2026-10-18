#ifndef VOLUME_CLOUD_PARAM_HLSLI
#define VOLUME_CLOUD_PARAM_HLSLI

// ボリューム雲の 3 つのパス(深度の縮小・雲の描画・拡大合成)が共有する定数バッファと、座標の変換。
// C++ 側の Renderer::VolumeCloudParam と同じ並び(16 バイト区切り)にすること。
// どのパスも同じ定数バッファを読む(パスごとに使う項目だけが違う)。

cbuffer VolumeCloudParam : register(b0) {
    row_major float4x4 gInvViewProj;
    float32_t3 gCameraPosition;
    float32_t gTime;
    float32_t3 gSunDirection;     // 太陽へ向かう向き(正規化済み)
    float32_t gExtinction;        // 消光係数: 密度 1 のとき、1 ユニット進むと光が減る割合(大きいほど濃い)
    float32_t3 gBoxCenter;
    float32_t gNoiseScale;        // 形のノイズの細かさ(ワールド 1 ユニットあたりのタイル数。小さいほど大きな雲)
    float32_t3 gBoxHalfSize;      // 箱の半分の大きさ(中心から面までの距離)
    float32_t gDetailScale;       // 細かい模様の細かさ(形のノイズに対する倍率)
    float32_t3 gWindVelocity;     // ノイズが流れる速さ(ワールドのユニット / 秒)
    float32_t gThreshold;         // 雲になるしきい値(大きいほど隙間が増える)
    float32_t gEdgeFade;          // 外形の縁を薄くする幅(0〜1)
    float32_t gErosion;           // 細かい模様で削る強さ
    float32_t gSteps;             // 視線のステップ数
    float32_t gLightSteps;        // 太陽方向のステップ数
    float32_t gLightStepLength;   // 太陽方向の 1 歩目の長さ(ユニット。以降は kLightStepGrowth 倍ずつ伸ばす)
    float32_t gPhaseG;            // 前方散乱の強さ(0 で均一、1 に近いほど太陽方向へ強く光る)
    float32_t gSunIntensity;      // 太陽の光の強さ
    float32_t gAmbientIntensity;  // 空からの光(環境光)の強さ
    float32_t2 gFullSize;         // シーンの描画先の大きさ(画素)
    float32_t2 gLowSize;          // 雲を描く低解像度の範囲の大きさ(画素)
    float32_t gDivisor;           // 解像度の分母(1 = 等倍、2 = 縦横半分)
    float32_t gShapeMode;         // 外形の種類: 0 = 箱(に内接する楕円体)、1 = 層(水平に無限に続く)
    float32_t gLayerBottom;       // 層: 下面の高さ(ワールド y)
    float32_t gLayerTop;          // 層: 上面の高さ(ワールド y)
    float32_t gMaxDistance;       // 層: 描く距離の上限(カメラから。端は霞へ溶かす)
    float32_t gFogDensity;        // 層: 距離に対する霞の強さ(大きいほど、近くから霞む)
    float32_t gHighlightKnee;     // 明るい部分の圧縮を始める値(0〜1)。これを超える光を、1 へ向けて滑らかに圧縮する(1 で圧縮なし)
    float32_t gShadowLift;        // 影の持ち上げ(0〜1)。大きいほど、影の部分に空からの明るい光が回る
};

static const float32_t kFarDepth = 0.99999f;           // これ以上の深度は、何も描かれていない(無限遠)
static const float32_t kInfiniteDistance = 100000.0f;  // 何も描かれていない所の、物体までの距離として使う値

// 画面の NDC(-1〜1、上が +y)から、ワールドの視線方向(正規化済み)を求める。
// 同じ画面位置の「近い点」と「遠い点」をワールドへ戻した差(Sky.PS と同じ)
float32_t3 RayDirectionFromNdc(float32_t2 ndc) {
    float32_t4 nearPos = mul(float32_t4(ndc, 0.0f, 1.0f), gInvViewProj);
    float32_t4 farPos = mul(float32_t4(ndc, 1.0f, 1.0f), gInvViewProj);
    return normalize(farPos.xyz / farPos.w - nearPos.xyz / nearPos.w);
}

// 深度から、カメラから物体までの距離を求める(何も描かれていない所は無限遠)
float32_t DistanceFromDepth(float32_t2 ndc, float32_t depth) {
    if (depth >= kFarDepth) {
        return kInfiniteDistance;
    }
    float32_t4 worldPos = mul(float32_t4(ndc, depth, 1.0f), gInvViewProj);
    return length(worldPos.xyz / worldPos.w - gCameraPosition);
}

#endif
