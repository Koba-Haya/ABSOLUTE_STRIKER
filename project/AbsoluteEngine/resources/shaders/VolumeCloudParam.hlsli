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
    // --- 着地の反応(層のみ。VolumeCloud.hlsli の ImpactDisplacement が使う)。gImpactStrength が 0 なら何もしない ---
    float32_t2 gImpactCenter;          // 着地点のワールド座標 (x, z)
    float32_t gImpactAge;              // 着地からの経過秒(負なら未着地)
    float32_t gImpactStrength;         // 反応の強さ(0 で無効。くぼみと波の高さに掛ける)
    float32_t gImpactCraterRadius;     // くぼみの半径(ガウス形の広がり)
    float32_t gImpactCraterDepth;      // くぼみの深さ(雲の面を押し下げる量)
    float32_t gImpactRingWidth;        // 波の縁の幅
    float32_t gImpactRingHeight;       // 波の縁の盛り上がり(雲の面を持ち上げる量)
    float32_t gImpactRingSpeed;        // 波の縁が外へ広がる速さ(ユニット / 秒)
    float32_t gImpactRefillSeconds;    // くぼみが雲で埋まり戻るまでの秒
    float32_t gImpactRingFadeSeconds;  // 波の縁が消えるまでの秒
    float32_t gSeaFill;                // 雲海を常に満たす量(0〜1。層のみ。0 で隙間のある雲海)
    float32_t gSeaFillThreshold;       // 満たしきったとき(gSeaFill = 1)の、雲になるしきい値(層のみ。小さいほど隙間が減る)
    float32_t gSeaFillRelief;          // 満たしたとき、大きな周期の濃淡を残す割合(0 = 全部均す、1 = 残す)。起伏(盛り上がり)を保つ
    float32_t gRimStrength;            // 層: 縁の光(太陽側の薄い所を明るくする)の強さ。0 で無効
    float32_t gRimWidth;               // 層: 縁の光が届く深さ(消光に掛ける倍率。小さいほど、深くまで届く)
    float32_t gRimPower;               // 層: 縁の光の鋭さ(大きいほど、縁の薄い所だけが光る)
    float32_t gMoundAmplitude;         // 層: 上面の起伏(盛り上がり)の大きさ(層の厚みに対する割合。山と谷の差)。0 で平ら
    float32_t gMoundScale;             // 層: 起伏の細かさ(形のノイズの細かさに対する倍率。大きいほど、小さな盛り上がり)
    float32_t gDebugView;              // 層: 確認用の表示(0 = 通常、1 = 光の厚み(1 - 透過率)、2 = 雲の表面の高さ)
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
