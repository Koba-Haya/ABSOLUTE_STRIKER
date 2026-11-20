#include "UiContinuePrompt.h"
#include <algorithm>
#include <cmath>

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

constexpr float kFontSize = 22.0f;          // 案内の文字の大きさ(px)
constexpr float kFadeInSeconds = 0.6f;      // 現れ始めから、点滅の始まりまでの、薄い -> 濃い時間(秒)
constexpr float kBlinkPeriodSeconds = 1.6f; // 点滅の 1 周期(秒)
constexpr float kMinBlinkAlpha = 0.35f;     // 点滅の、いちばん薄いときの濃さ
constexpr float kTwoPi = 6.28318530718f;
constexpr float kHalf = 0.5f;
// 文字の色(白。背景の空の上でも読めるよう、不透明度だけを変える)
const Vector4 kTextColor{1.0f, 1.0f, 1.0f, 1.0f};
} // namespace

void UiContinuePrompt::Initialize() { font_.Initialize(kFontAtlasPath, kFontMetricsPath); }

void UiContinuePrompt::SetPosition(float centerX, float centerY) {
  centerX_ = centerX;
  centerY_ = centerY;
}

void UiContinuePrompt::Show(const std::string &text) {
  if (visible_) return;
  text_ = text;
  time_ = 0.0f;
  visible_ = true;
}

void UiContinuePrompt::Hide() { visible_ = false; }

void UiContinuePrompt::Update(float deltaTime) {
  if (!visible_) return;
  time_ += deltaTime;
}

void UiContinuePrompt::Draw() {
  if (!visible_) return;
  // 点滅: 0.5 + 0.5 * sin で 0 から 1 を往復させ、最小の濃さから 1 の間へ写す。現れ始めは、フェードインを掛ける
  const float wave = kHalf + kHalf * std::sin(kTwoPi * time_ / kBlinkPeriodSeconds);
  const float blink = kMinBlinkAlpha + (1.0f - kMinBlinkAlpha) * wave;
  const float fadeIn = std::clamp(time_ / kFadeInSeconds, 0.0f, 1.0f);
  Vector4 color = kTextColor;
  color.w = blink * fadeIn;
  // 行ボックスの高さの半分だけ上へずらして、centerY を文字の縦の中心にする
  const float top = centerY_ - font_.GetLineHeight(kFontSize) * kHalf;
  font_.Draw(text_, {centerX_, top}, kFontSize, color, BitmapFont::Align::Center);
}
