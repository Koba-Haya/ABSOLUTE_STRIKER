#pragma once
#include "Vector.h"

/// <summary>
/// ゲーム中の HUD(進行バー、HP バー、ロックオンの枠など)で共通に使う、見た目の寸法と色。
/// 分けた理由: 小さい HUD の部品が、同じトーン(寸法、色)をそろえて使うため。部品ごとに同じ値を持つと、
/// 片方だけ変えてずれてしまう。見た目を変えたいときは、ここだけを直す。
/// </summary>
namespace HudStyle {
// --- 寸法(px) ---
inline constexpr float kScreenMargin = 20.0f;   // 画面の端から HUD までの間隔
inline constexpr float kLabelFontSize = 16.0f;  // 小さな見出しの文字の大きさ
inline constexpr float kLabelGap = 3.0f;        // 見出しの文字と、バー(枠)の間隔
inline constexpr float kBorderThickness = 1.0f; // バー(枠)の線の太さ
inline constexpr float kFillInset = 2.0f;       // 枠と中身の間隔
inline constexpr float kBackingPadding = 6.0f;  // 見出しとバーの全体を囲む下地の、外側への余白

// --- 色(RGBA) ---
inline constexpr Vector4 kLabelColor{0.90f, 0.95f, 1.00f, 1.00f};
inline constexpr Vector4 kBackingColor{0.04f, 0.09f, 0.16f, 0.62f}; // 見出しとバーの全体の下地(明るい雲の上でも読めるように)
inline constexpr Vector4 kBackColor{0.02f, 0.06f, 0.12f, 0.60f};    // バーの背景
inline constexpr Vector4 kBorderColor{0.62f, 0.86f, 1.00f, 0.70f};  // 枠
inline constexpr Vector4 kAccentColor{0.62f, 0.86f, 1.00f, 0.95f};  // 通常の中身(明るい青)
inline constexpr Vector4 kWarnColor{1.00f, 0.70f, 0.20f, 0.95f};    // 注意の色(橙。HP が少ない、ロックオンが上限、など)

/// 色のアルファに倍率を掛けた色を返す
inline Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }
} // namespace HudStyle
