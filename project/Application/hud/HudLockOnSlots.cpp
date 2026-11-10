#include "HudLockOnSlots.h"
#include "../ScreenSize.h"
#include "BitmapFont.h"
#include "HudStyle.h"
#include <algorithm>
#include <vector>

namespace {
// --- 寸法(px) ---
constexpr float kSlotHeight = 10.0f;      // 枠の高さ(HP バーと同じ)
constexpr float kSlotMaxWidth = 40.0f;    // 1 つの枠の幅の上限(枠が少ないときは、この幅)
constexpr float kSlotGap = 5.0f;          // 枠と枠の間隔
constexpr float kTotalMaxWidth = 300.0f;  // 枠の並び全体の幅の上限(HP バーと同じ。枠が多いときは、枠の幅を詰めてこの中に収める)

// --- 文字列 ---
constexpr const char *kLabelText = "ロックオン";
} // namespace

void HudLockOnSlots::Initialize(BitmapFont *font) {
  font_ = font;
  rects_.Initialize();
}

void HudLockOnSlots::Draw(int lockedCount, int maxCount, float alpha) {
  if (!font_ || maxCount <= 0 || alpha <= 0.0f) return;
  const int filled = std::clamp(lockedCount, 0, maxCount);
  const bool isFull = filled >= maxCount;

  // 1. 枠の幅を決める。枠が多いときは、並び全体が kTotalMaxWidth に収まるように詰める
  const float gapsWidth = kSlotGap * static_cast<float>(maxCount - 1);
  const float slotWidth = (std::min)(kSlotMaxWidth, (kTotalMaxWidth - gapsWidth) / static_cast<float>(maxCount));
  const float slotsWidth = slotWidth * static_cast<float>(maxCount) + gapsWidth;

  // 2. 配置: 右下。画面の右端(余白の内側)にそろえる。見出しの方が広いときは、見出しの幅を、全体の幅にする
  const float labelWidth = font_->Measure(kLabelText, HudStyle::kLabelFontSize).x;
  const float contentWidth = (std::max)(slotsWidth, labelWidth);
  const float left = ScreenSize::kWidth - HudStyle::kScreenMargin - contentWidth;
  const float slotTop = ScreenSize::kHeight - HudStyle::kScreenMargin - kSlotHeight;
  const float labelY = slotTop - HudStyle::kLabelGap - font_->GetLineHeight(HudStyle::kLabelFontSize);

  // 3. 下地(見出しと枠の全体を囲む。文字より先に描く)
  const float backingTop = labelY - HudStyle::kBackingPadding;
  rects_.Fill(UiRect{left - HudStyle::kBackingPadding, backingTop, contentWidth + HudStyle::kBackingPadding * 2.0f,
                     slotTop + kSlotHeight + HudStyle::kBackingPadding - backingTop},
              HudStyle::WithAlphaScale(HudStyle::kBackingColor, alpha));

  // 4. 見出し
  font_->Draw(kLabelText, {left, labelY}, HudStyle::kLabelFontSize, HudStyle::WithAlphaScale(HudStyle::kLabelColor, alpha));

  // 5. 枠の並び。背景、枠線、埋まった枠の中身を、それぞれ 1 回の描画にまとめる
  const Vector4 &activeColor = isFull ? HudStyle::kWarnColor : HudStyle::kAccentColor;
  const Vector4 &lineColor = isFull ? HudStyle::kWarnColor : HudStyle::kBorderColor;
  std::vector<UiRect> backs, lines, fills;
  backs.reserve(maxCount);
  lines.reserve(static_cast<size_t>(maxCount) * 4);
  fills.reserve(maxCount);
  const float t = HudStyle::kBorderThickness;
  for (int i = 0; i < maxCount; ++i) {
    const float x = left + (slotWidth + kSlotGap) * static_cast<float>(i);
    backs.push_back({x, slotTop, slotWidth, kSlotHeight});
    lines.push_back({x, slotTop, slotWidth, t});                         // 上
    lines.push_back({x, slotTop + kSlotHeight - t, slotWidth, t});       // 下
    lines.push_back({x, slotTop, t, kSlotHeight});                       // 左
    lines.push_back({x + slotWidth - t, slotTop, t, kSlotHeight});       // 右
    if (i < filled) {
      fills.push_back({x + HudStyle::kFillInset, slotTop + HudStyle::kFillInset, slotWidth - HudStyle::kFillInset * 2.0f,
                       kSlotHeight - HudStyle::kFillInset * 2.0f});
    }
  }
  rects_.Fill(backs.data(), backs.size(), HudStyle::WithAlphaScale(HudStyle::kBackColor, alpha));
  rects_.Fill(lines.data(), lines.size(), HudStyle::WithAlphaScale(lineColor, alpha));
  if (!fills.empty()) rects_.Fill(fills.data(), fills.size(), HudStyle::WithAlphaScale(activeColor, alpha));
}
