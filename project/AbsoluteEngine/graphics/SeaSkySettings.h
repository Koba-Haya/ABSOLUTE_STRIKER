#pragma once
#include "Method.h"
#include <cmath>

// 新方式の雲海の背景(空)の見た目のパラメータ。描画(Renderer::DrawSeaSky)とデバッグ UI が参照するデータだけの構造体。
// 空のグラデーション・太陽・雲の切れ間から見える下の層を、雲のレイマーチなしで描く(雲そのものは、ボリューム雲の層が担う)。
// 描画の仕組みは shaders/SeaSky.PS.hlsl を参照。
struct SeaSkySettings {
  float sunAzimuthDeg = 35.0f;   // 太陽の方位(度。+z 方向が 0、+x 方向が正)。旧方式の雲海と同じ既定値
  float sunElevationDeg = 25.0f; // 太陽の高さ(度)
  float gapDarkness = 0.50f;     // 雲の切れ間から見える下の層の暗さ(0〜1)
  float floorHeight = -70.0f;    // 切れ間から見える下の層の高さ(ワールド y。雲の層より十分下にする)

  // 太陽へ向かう向き(正規化済み)。ボリューム雲の光の向きにも使って、空と雲の光の向きをそろえる
  Vector3 SunDirection() const {
    constexpr float kDegToRad = 3.14159265f / 180.0f;
    const float azimuth = sunAzimuthDeg * kDegToRad;
    const float elevation = sunElevationDeg * kDegToRad;
    return {std::sin(azimuth) * std::cos(elevation), std::sin(elevation), std::cos(azimuth) * std::cos(elevation)};
  }
};
