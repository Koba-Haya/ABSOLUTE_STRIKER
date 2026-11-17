#include "UpgradeToast.h"

#include <algorithm>

#include "../ScreenSize.h"

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

// --- 配置(px) ---
constexpr float kBaseY = 120.0f;         // いちばん新しい知らせの、上端の高さ(上の進行バーの下)
constexpr float kLineStep = 40.0f;       // 知らせどうしの間隔(新しいものが上、古いものが下)
constexpr float kFontSize = 26.0f;       // 文字の大きさ
constexpr float kPlatePaddingX = 18.0f;  // 板の、文字の左右の余白
constexpr float kPlatePaddingY = 3.0f;   // 板の、文字の上下の余白
constexpr float kRiseDistance = 12.0f;   // 現れるとき、下から上がってくる距離
constexpr float kHalf = 0.5f;

// --- 時間(秒) ---
constexpr float kFadeInSeconds = 0.15f;  // 現れる時間
constexpr float kHoldSeconds = 1.6f;     // 留まる時間(現れ終わってから)
constexpr float kFadeOutSeconds = 0.40f; // 薄れて消える時間
constexpr float kOverflowFadeSeconds = 0.15f; // あふれた古い知らせが、薄れて消える時間
constexpr size_t kMaxToasts = 4;         // 同時に出す知らせの最大数

// --- 色(RGBA) ---
const Vector4 kTextColor{1.00f, 0.92f, 0.62f, 1.00f}; // 文字(金色)
const Vector4 kPlateColor{0.05f, 0.10f, 0.18f, 0.55f}; // 板(半透明の濃い青)

float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }

// 知らせの全寿命(現れる + 留まる + 薄れる)
constexpr float kLifeSeconds = kFadeInSeconds + kHoldSeconds + kFadeOutSeconds;
} // namespace

void UpgradeToast::Initialize() {
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);
  rects_.Initialize();
  toasts_.clear();
  ready_ = true;
}

void UpgradeToast::Show(const std::string &text) {
  if (!ready_) return;
  toasts_.push_front({text, 0.0f});
}

void UpgradeToast::Update(float deltaTime) {
  for (Toast &toast : toasts_) {
    toast.age += deltaTime;
  }
  // 寿命が尽きたものを消す
  while (!toasts_.empty() && toasts_.back().age >= kLifeSeconds) {
    toasts_.pop_back();
  }
}

void UpgradeToast::Draw() {
  if (!ready_ || toasts_.empty()) return;
  const float centerX = ScreenSize::kWidth * kHalf;
  const float lineHeight = font_.GetLineHeight(kFontSize);

  for (size_t i = 0; i < toasts_.size(); ++i) {
    const Toast &toast = toasts_[i];
    // 濃さ: 現れる -> 留まる -> 薄れる。同時の最大数を超えた古いものは、すぐ薄れる
    float alpha = Saturate(toast.age / kFadeInSeconds);
    if (toast.age > kFadeInSeconds + kHoldSeconds) {
      alpha *= 1.0f - Saturate((toast.age - kFadeInSeconds - kHoldSeconds) / kFadeOutSeconds);
    }
    if (i >= kMaxToasts) {
      alpha *= 1.0f - Saturate(toast.age / kOverflowFadeSeconds); // あふれた分は、描かないに近い(すぐ消える)
    }
    if (alpha <= 0.0f) continue;

    // 少し下から上がって現れる
    const float rise = (1.0f - Saturate(toast.age / kFadeInSeconds)) * kRiseDistance;
    const float y = kBaseY + static_cast<float>(i) * kLineStep + rise;
    const float textWidth = font_.Measure(toast.text, kFontSize).x;
    rects_.Fill(UiRect{centerX - textWidth * kHalf - kPlatePaddingX, y - kPlatePaddingY, textWidth + kPlatePaddingX * 2.0f,
                       lineHeight + kPlatePaddingY * 2.0f},
                WithAlphaScale(kPlateColor, alpha));
    font_.Draw(toast.text, {centerX, y}, kFontSize, WithAlphaScale(kTextColor, alpha), BitmapFont::Align::Center);
  }
}
