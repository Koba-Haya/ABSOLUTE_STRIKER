#ifndef TRANSITION_PARAM_HLSLI
#define TRANSITION_PARAM_HLSLI

// 画面遷移の最終パス(Transition.PS.hlsl と、2 つ目のレンズ水滴のパス LensDrops.PS.hlsl)が共有する定数バッファ。
// C++ の Renderer::TransitionParam と同じ並び(16 バイト区切り)にすること。
// gKind: C++ の TransitionKind と同じ並び
//   0 = フェード、1 = ノイズワイプ、2 = ズームブラー、3 = 水の波紋、4 = レンズの水滴、5 = 雲突入、6 = 霧(水滴は LensDrops.PS.hlsl)
cbuffer TransitionParam : register(b0) {
    row_major float4x4 gInvViewProj; // 描画したシーンのカメラ(世界座標の視線の復元・深度から位置への変換用)
    row_major float4x4 gViewProj;    // 太陽の画面位置(レンズフレア用)
    float32_t3 gCameraPosition;
    float32_t gSlabBottom;           // 雲を抜ける遷移: 雲の層の下面・上面(今のシーンの世界の高さ)
    float32_t3 gSunDirection;        // 太陽へ向かう向き(今のシーンの世界)
    float32_t gSlabTop;
    float32_t3 gColor;               // 覆う色(リニア)
    float32_t gCover;                // 覆い具合(0 = 透明、1 = 完全に覆う)
    float32_t gKind;
    float32_t gSoftness;             // ノイズワイプ: 縁のやわらかさ
    float32_t gNoiseScale;           // ノイズワイプ: 画面の高さあたりのマス数
    float32_t gBlurStrength;         // ズームブラー: 覆い具合が最大のときの引き込む割合
    float32_t2 gCenter;              // ズームブラー・波紋: 中心(UV)
    float32_t gAspect;               // 画面の縦横比(横 / 縦)。縦横に伸びないように使う
    float32_t gTime;                 // 遷移の開始からの経過(秒)。覆い具合と違い、晴れる間も増え続ける
    float32_t gDuration;             // 遷移全体の長さ(秒)。雲を抜ける遷移の進行(経過 / 全体)に使う
    float32_t3 gPadding;
    float32_t4 gMistFog;             // 霧: (彩度の落とし量, ぼかしの最大半径(画面の高さを 1 とする), 覆い具合 1 の視程, 覆い具合 0 の視程)
    float32_t4 gMistFog2;            // 霧: (深度の無い背景の距離, 最後の覆いを始める覆い具合, 太陽側の明るさ, デバッグ表示の番号)
    float32_t4 gMistDrop;            // 水滴: (濡れ具合 0〜1, 霧の倍率 0〜1, 水滴が付き始めてからの秒, レンズ水滴のパスを使うか 0 / 1)
    float32_t4 gMistPlane;           // 霧: (深度の無い背景の水平面の高さ, 水平面を使うか 0 / 1, 未使用, 未使用)
};

#endif
