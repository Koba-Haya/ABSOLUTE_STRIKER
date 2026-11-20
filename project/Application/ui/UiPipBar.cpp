#include "UiPipBar.h"

#include <algorithm>
#include <vector>

#include "../hud/HudStyle.h"

float UiPipBar::GetWidth(int count, const Style &style) {
  if (count <= 0) return 0.0f;
  return style.pipSize.x * static_cast<float>(count) + style.gap * static_cast<float>(count - 1);
}

void UiPipBar::Initialize() { rects_.Initialize(); }

void UiPipBar::Draw(const Vector2 &topLeft, int count, float baseValue, float gainValue, const Style &style) {
  if (count <= 0 || style.alpha <= 0.0f) return;

  // 値は、0 から count の範囲に収める。増える分は、いまの値の続きから、count を超えない範囲まで
  const float maxValue = static_cast<float>(count);
  const float baseEnd = std::clamp(baseValue, 0.0f, maxValue);
  const float gainEnd = std::clamp(baseEnd + (std::max)(gainValue, 0.0f), baseEnd, maxValue);

  // 背景、枠線、いまの値、増える分を、それぞれ 1 回の描画にまとめる(HudLockOnSlots と同じ方針)
  std::vector<UiRect> backs, lines, bases, gains;
  backs.reserve(static_cast<size_t>(count));
  lines.reserve(static_cast<size_t>(count) * 4);
  const float t = HudStyle::kBorderThickness;
  const float inset = HudStyle::kFillInset;
  const float innerWidth = style.pipSize.x - inset * 2.0f;
  const float innerHeight = style.pipSize.y - inset * 2.0f;
  for (int i = 0; i < count; ++i) {
    const float x = topLeft.x + (style.pipSize.x + style.gap) * static_cast<float>(i);
    const float y = topLeft.y;
    backs.push_back({x, y, style.pipSize.x, style.pipSize.y});
    lines.push_back({x, y, style.pipSize.x, t});                         // 上
    lines.push_back({x, y + style.pipSize.y - t, style.pipSize.x, t});   // 下
    lines.push_back({x, y, t, style.pipSize.y});                         // 左
    lines.push_back({x + style.pipSize.x - t, y, t, style.pipSize.y});   // 右

    // この枠([i, i + 1))の中で、いまの値が届く割合と、増える分が届く割合
    const float baseFrac = std::clamp(baseEnd - static_cast<float>(i), 0.0f, 1.0f);
    const float gainFrac = std::clamp(gainEnd - static_cast<float>(i), 0.0f, 1.0f);
    if (baseFrac > 0.0f) {
      bases.push_back({x + inset, y + inset, innerWidth * baseFrac, innerHeight});
    }
    if (gainFrac > baseFrac) {
      gains.push_back({x + inset + innerWidth * baseFrac, y + inset, innerWidth * (gainFrac - baseFrac), innerHeight});
    }
  }
  rects_.Fill(backs.data(), backs.size(), HudStyle::WithAlphaScale(HudStyle::kBackColor, style.alpha));
  rects_.Fill(lines.data(), lines.size(), HudStyle::WithAlphaScale(HudStyle::kBorderColor, style.alpha));
  if (!bases.empty()) rects_.Fill(bases.data(), bases.size(), HudStyle::WithAlphaScale(style.baseColor, style.alpha));
  if (!gains.empty()) rects_.Fill(gains.data(), gains.size(), HudStyle::WithAlphaScale(style.gainColor, style.alpha * style.gainBlink));
}
