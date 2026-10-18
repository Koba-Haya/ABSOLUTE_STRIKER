#pragma once
#include "Method.h"
#include <cmath>
#include <cstdint>

// 雲海(ゲーム中に、自機の下に広がる雲の海)の見た目のパラメータ。
// 描画(Renderer::DrawCloudSea)とデバッグ UI(CloudSeaDebugUi)の両方が参照するデータだけの構造体。
// 既定値は、雲海チューナー(プレビュー)で調整して決めた値。
// 描画の仕組み(高さ場レイマーチ)は shaders/CloudSea.hlsli を参照。
struct CloudSeaSettings {
  // --- 雲の形 ---
  float thickness = 9.0f;  // 雲の厚み(下へ。ワールドのユニット)
  float bump = 4.5f;       // 起伏の高さ(上へ。ユニット)
  float scale = 0.065f;    // ノイズの細かさ(1 ユニットあたりのマス数。小さいほど大きな雲)
  float threshold = 0.26f; // 雲になるしきい値(小さいほど雲が多い)
  float softness = 0.40f;  // 縁のやわらかさ

  // --- 形の見直し(大きなうねり・折り目) ---
  float swellHeight = 8.0f; // 大きなうねり(山と谷)の高さ(ユニット)
  float swellBlend = 0.50f; // うねりで雲の量をずらす強さ(0〜1)
  float billow = 0.50f;     // 細かい凹凸の折り目(もこもこ。0〜1)

  // --- 陰影と色 ---
  float shadowStrength = 0.60f; // 雲同士の影の強さ(0〜1)
  float shadowLength = 40.0f;   // 影を調べる距離(ユニット)
  float gapDarkness = 0.50f;    // 切れ間から見える下の雲層の暗さ(0〜1)
  float aerial = 0.70f;         // 空気遠近(遠くを青紫へ寄せる強さ。0〜1)
  float contrast = 0.35f;       // コントラスト(0〜1)

  // --- 見た目 ---
  float density = 0.40f; // 濃さ(厚みの見え方。大きいほど不透明)
  float fog = 0.0010f;   // 霞(遠くを背景へ溶かす強さ)
  float sunAzimuthDeg = 35.0f;   // 太陽の方位(度。+z 方向が 0、+x 方向が正)
  float sunElevationDeg = 25.0f; // 太陽の高さ(度)

  // --- 空だけ描く(雲をボリューム雲の層が担うときの切り替え。シーンが毎フレーム設定する) ---
  bool skyOnly = false; // true なら、雲のレイマーチを飛ばして、空(グラデーション・太陽・切れ間の下の層)だけ描く

  // --- 配置・負荷 ---
  float seaHeight = -35.5f; // 雲海の平均の高さ(ワールド y)。レールの y=0 に対して、自機の 35 ユニット下
  int32_t steps = 14;       // レイマーチのステップ数(多いほど重く、きれい)
  bool jitter = false;      // ステップの開始位置を画素ごとにずらす(縞を砂目に変える)

  // 太陽へ向かう向き(正規化済み)
  Vector3 SunDirection() const {
    constexpr float kDegToRad = 3.14159265f / 180.0f;
    const float azimuth = sunAzimuthDeg * kDegToRad;
    const float elevation = sunElevationDeg * kDegToRad;
    return {std::sin(azimuth) * std::cos(elevation), std::sin(elevation), std::cos(azimuth) * std::cos(elevation)};
  }
};
