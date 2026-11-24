#include "ExhibitionHint.h"

#include <chrono>
#include <cmath>

#include "../ScreenSize.h"

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

// --- 配置(px)---
constexpr float kFontSize = 13.0f;
constexpr float kRightMargin = 20.0f; // 画面の右端からの間隔(HUD の余白と同じ)
constexpr float kTop = 3.0f;          // 画面の上端から、下地の上端までの間隔(HUD は、上端から 20px の内側)
constexpr float kPaddingX = 6.0f;     // 下地の、文字の左右の余白
constexpr float kPaddingY = 1.0f;     // 下地の、文字の上下の余白

// --- 点滅 ---
constexpr float kBlinkPeriodSeconds = 1.8f;
constexpr float kBlinkMinAlpha = 0.35f; // いちばん薄いときの濃さ
constexpr float kTwoPi = 6.28318530718f;
constexpr float kHalf = 0.5f;

// --- 文字列と色 ---
constexpr const char *kTextNormal = "TAB  展示モード";
constexpr const char *kTextExhibition = "TAB  展示モードを終了";
const Vector4 kTextColor{1.0f, 1.0f, 1.0f, 1.0f};
const Vector4 kBackingColor{0.04f, 0.09f, 0.16f, 0.55f}; // 明るい雲や空の上でも読めるように、暗い下地を敷く
} // namespace

void ExhibitionHint::Initialize() {
  if (ready_) return;
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);
  rects_.Initialize();
  ready_ = true;
}

void ExhibitionHint::Draw(bool exhibitionEnabled) {
  if (!ready_) return;

  // 点滅: 0 から 1 を往復する波を、最小の濃さから 1 の間へ写す。時刻は、実時間で測る(シーンをまたいでも、途切れない)
  static const auto start = std::chrono::steady_clock::now();
  const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
  const float wave = kHalf + kHalf * std::sin(kTwoPi * t / kBlinkPeriodSeconds);
  const float alpha = kBlinkMinAlpha + (1.0f - kBlinkMinAlpha) * wave;

  const char *text = exhibitionEnabled ? kTextExhibition : kTextNormal;
  const float textWidth = font_.Measure(text, kFontSize).x;
  const float lineHeight = font_.GetLineHeight(kFontSize);
  const float right = ScreenSize::kWidth - kRightMargin;
  const float left = right - textWidth - kPaddingX * 2.0f;

  Vector4 backing = kBackingColor;
  backing.w *= alpha;
  rects_.Fill(UiRect{left, kTop, textWidth + kPaddingX * 2.0f, lineHeight + kPaddingY * 2.0f}, backing);
  Vector4 color = kTextColor;
  color.w = alpha;
  font_.Draw(text, {right - kPaddingX, kTop + kPaddingY}, kFontSize, color, BitmapFont::Align::Right);
}
