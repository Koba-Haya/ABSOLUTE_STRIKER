#include "HudLockOnMarkers.h"
#include "HudStyle.h"
#include <cmath>

namespace {
// --- 寸法(px) ---
constexpr float kRingThickness = 2.0f;     // 輪の線の太さ(細い輪)
constexpr float kBracketHalfSize = 22.0f;  // カッコの四隅が作る正方形の、半分の大きさ
constexpr float kBracketArmLength = 9.0f;  // カッコの 1 辺(L 字の腕)の長さ
constexpr float kBracketThickness = 2.0f;  // カッコの線の太さ
constexpr float kHalf = 0.5f;

// --- 色(RGBA) ---
// 輪とカッコは、通常は明るい青、上限に達したら橙(HudStyle のトーンにそろえる)。輪は控えめに薄くする
constexpr float kRingAlphaScale = 0.75f;
} // namespace

void HudLockOnMarkers::Initialize() { renderer_.Initialize(); }

void HudLockOnMarkers::BuildRing_(const Vector2 &center, float radius) {
  // 輪 = 外側の円(半径 outer)から、内側の円(半径 inner)を引いたもの。
  // 1 ピクセルの高さの行ごとに、行の中心の高さで円を切った断面を求める(円の式 x^2 + y^2 = r^2 から x を出す)
  const float outer = radius + kRingThickness * kHalf;
  const float inner = radius - kRingThickness * kHalf;
  const int firstRow = static_cast<int>(std::floor(center.y - outer));
  const int lastRow = static_cast<int>(std::ceil(center.y + outer));
  for (int row = firstRow; row < lastRow; ++row) {
    const float dy = (static_cast<float>(row) + kHalf) - center.y; // 行の中心の、円の中心からの縦の距離
    if (std::fabs(dy) >= outer) continue;
    const float outerX = std::sqrt(outer * outer - dy * dy);
    const float y = static_cast<float>(row);
    if (std::fabs(dy) < inner) {
      // 内側の円も横切る行: 左右の 2 か所(外の端から内の端まで)
      const float innerX = std::sqrt(inner * inner - dy * dy);
      rects_.push_back({center.x - outerX, y, outerX - innerX, 1.0f});
      rects_.push_back({center.x + innerX, y, outerX - innerX, 1.0f});
    } else {
      // 輪の上と下の端: 内側の円に届かないので、全体を 1 本で
      rects_.push_back({center.x - outerX, y, outerX * 2.0f, 1.0f});
    }
  }
}

void HudLockOnMarkers::AddBrackets_(const Vector2 &center) {
  const float left = center.x - kBracketHalfSize;
  const float right = center.x + kBracketHalfSize;
  const float top = center.y - kBracketHalfSize;
  const float bottom = center.y + kBracketHalfSize;
  const float arm = kBracketArmLength;
  const float t = kBracketThickness;
  // 四隅の L 字(各隅に、横の腕と縦の腕)
  rects_.push_back({left, top, arm, t});
  rects_.push_back({left, top, t, arm});
  rects_.push_back({right - arm, top, arm, t});
  rects_.push_back({right - t, top, t, arm});
  rects_.push_back({left, bottom - t, arm, t});
  rects_.push_back({left, bottom - arm, t, arm});
  rects_.push_back({right - arm, bottom - t, arm, t});
  rects_.push_back({right - t, bottom - arm, t, arm});
}

void HudLockOnMarkers::Draw(bool lockingMode, const Vector2 &cursorPos, float radius, const std::vector<Vector2> &targets,
                            bool isFull) {
  if (!lockingMode) return;
  const Vector4 &base = isFull ? HudStyle::kWarnColor : HudStyle::kAccentColor;

  // 輪(1 回の描画)
  rects_.clear();
  BuildRing_(cursorPos, radius);
  renderer_.Fill(rects_.data(), rects_.size(), HudStyle::WithAlphaScale(base, kRingAlphaScale));

  // カッコ(全員ぶんを 1 回の描画に)
  if (!targets.empty()) {
    rects_.clear();
    for (const Vector2 &target : targets) AddBrackets_(target);
    renderer_.Fill(rects_.data(), rects_.size(), base);
  }
}
