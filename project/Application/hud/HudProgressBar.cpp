#include "HudProgressBar.h"
#include "BitmapFont.h"
#include "HudStyle.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
// --- 寸法(px) ---
// 見出しの文字、枠、下地の寸法と色は、ほかの HUD 部品とそろえるため HudStyle の値を使う
constexpr float kLabelFontSize = HudStyle::kLabelFontSize;
constexpr float kLabelGap = HudStyle::kLabelGap;
constexpr float kBorderThickness = HudStyle::kBorderThickness;
constexpr float kFillInset = HudStyle::kFillInset;
constexpr float kBackingPadding = HudStyle::kBackingPadding; // 文字とバーの全体を囲む下地の、外側への余白(明るい背景でも文字が読めるようにする)

// --- 文字列 ---
constexpr const char *kPercentFormat = "%3d%%"; // 数字は固定幅なので、3 桁に揃えれば桁がぶれない
constexpr size_t kPercentBufferSize = 16;
constexpr float kPercentScale = 100.0f;
constexpr int kPercentMax = 100;

// --- 色(RGBA) ---
constexpr const Vector4 &kLabelColor = HudStyle::kLabelColor;
constexpr const Vector4 &kBackingColor = HudStyle::kBackingColor; // 文字とバーの全体の下地
constexpr const Vector4 &kBackColor = HudStyle::kBackColor;
constexpr const Vector4 &kBorderColor = HudStyle::kBorderColor;

/// 割合(0 から 1)を整数のパーセントにする。四捨五入だが、「まだ残っているのに 0%」「満タンでないのに 100%」には見せない
/// (最大値が大きい値、たとえば HP 100 のときに、残りが少ない状態が分かるようにするため)
int ToPercent(float ratio) {
  int percent = static_cast<int>(std::lround(ratio * kPercentScale));
  if (ratio > 0.0f) percent = (std::max)(percent, 1);
  if (ratio < 1.0f) percent = (std::min)(percent, kPercentMax - 1);
  return percent;
}

/// 色のアルファに倍率を掛けた色を返す
Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }
} // namespace

void HudProgressBar::Initialize(BitmapFont *font) {
  font_ = font;
  rects_.Initialize();
}

void HudProgressBar::SetBounds(const Vector2 &position, const Vector2 &size) {
  position_ = position;
  size_ = size;
}

void HudProgressBar::Draw(float ratioValue, float alpha, const char *label, const Vector4 &fillColor) {
  if (!font_ || alpha <= 0.0f) return;
  const float ratio = std::clamp(ratioValue, 0.0f, 1.0f);

  const float labelY = position_.y - kLabelGap - font_->GetLineHeight(kLabelFontSize);

  // 1. 下地(文字とバーの全体を囲む暗い四角。雲のような明るい背景の上でも読めるようにする。文字より先に描く)
  const float backingTop = labelY - kBackingPadding;
  rects_.Fill(UiRect{position_.x - kBackingPadding, backingTop, size_.x + kBackingPadding * 2.0f,
                     position_.y + size_.y + kBackingPadding - backingTop},
              WithAlphaScale(kBackingColor, alpha));

  // 2. 文字: バーの上に、左へ見出し、右へパーセント
  font_->Draw(label, {position_.x, labelY}, kLabelFontSize, WithAlphaScale(kLabelColor, alpha));
  char percent[kPercentBufferSize];
  std::snprintf(percent, sizeof(percent), kPercentFormat, ToPercent(ratio));
  font_->Draw(percent, {position_.x + size_.x, labelY}, kLabelFontSize, WithAlphaScale(kLabelColor, alpha),
              BitmapFont::Align::Right);

  // 3. バーの背景と、枠(上下左右の 4 本の線。同じ色なので 1 回の描画にまとまる)
  rects_.Fill(UiRect{position_.x, position_.y, size_.x, size_.y}, WithAlphaScale(kBackColor, alpha));
  const UiRect border[] = {
      {position_.x, position_.y, size_.x, kBorderThickness},                                  // 上
      {position_.x, position_.y + size_.y - kBorderThickness, size_.x, kBorderThickness},     // 下
      {position_.x, position_.y, kBorderThickness, size_.y},                                  // 左
      {position_.x + size_.x - kBorderThickness, position_.y, kBorderThickness, size_.y},     // 右
  };
  rects_.Fill(border, sizeof(border) / sizeof(border[0]), WithAlphaScale(kBorderColor, alpha));

  // 4. 中身(進行度に応じた長さ。枠の内側に収める)
  const float innerWidth = size_.x - kFillInset * 2.0f;
  const float innerHeight = size_.y - kFillInset * 2.0f;
  if (ratio > 0.0f && innerWidth > 0.0f && innerHeight > 0.0f) {
    rects_.Fill(UiRect{position_.x + kFillInset, position_.y + kFillInset, innerWidth * ratio, innerHeight},
                WithAlphaScale(fillColor, alpha));
  }
}
