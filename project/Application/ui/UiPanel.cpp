#include "UiPanel.h"
#include "BitmapFont.h"
#include <algorithm>
#include <iterator>

namespace {
// --- 出現の動き ---
// 背景が開き終わるまでの時間(秒)
constexpr float kOpenDurationSec = 0.45f;
// 中身(タイトル、飾り)が現れ始める進み具合(0 から 1)。背景がほぼ開き切ってから現れるようにする
constexpr float kContentStartProgress = 0.75f;
// 開き始めの背景の高さの割合の下限(0 だと最初の 1 フレームに何も見えない。細い線から開く)
constexpr float kMinOpenness = 0.01f;

// --- 寸法(px) ---
constexpr float kBorderThickness = 2.0f; // 枠の線の太さ
constexpr float kTitleBarHeight = 48.0f; // タイトルバーの高さ
constexpr float kAccentThickness = 2.0f; // タイトルバーの下の明るい線の太さ
constexpr float kTitleFontSize = 26.0f;  // タイトルの文字の大きさ
constexpr float kTitlePaddingX = 20.0f;  // タイトルの左の余白
constexpr float kBodyPadding = 24.0f;    // 本体領域の内側の余白
constexpr float kCornerLength = 18.0f;   // 四隅の飾りの長さ
constexpr float kCornerThickness = 4.0f; // 四隅の飾りの太さ
constexpr float kHalf = 0.5f;

// --- 色(RGBA) ---
const Vector4 kBackgroundColor{0.04f, 0.08f, 0.14f, 0.78f}; // 暗い青の半透明
const Vector4 kBorderColor{0.45f, 0.65f, 0.80f, 0.60f};     // 細い枠(控えめ)
const Vector4 kTitleBarColor{0.11f, 0.24f, 0.38f, 0.90f};   // 濃い青のタイトルバー
const Vector4 kAccentColor{0.62f, 0.86f, 1.00f, 0.95f};     // 明るい線と四隅の飾り
const Vector4 kTitleTextColor{0.93f, 0.97f, 1.00f, 1.00f};  // タイトルの文字

/// 0 から 1 の値 t を、最初が速く終わりがゆっくりな動きにする(ease-out cubic)
float EaseOutCubic(float t) {
  const float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

// 線の太さの下限(px)。倍率で細くしても、これより細くしない(1 px 未満だと線が消えたり、ちらついたりする)
constexpr float kMinLineThickness = 1.0f;

/// 線の太さに倍率を掛け、下限で止める
float ScaledThickness(float thickness, float scale) { return (std::max)(thickness * scale, kMinLineThickness); }

/// 色のアルファに倍率を掛けた色を返す
Vector4 WithAlphaScale(const Vector4 &color, float scale) {
  return {color.x, color.y, color.z, color.w * scale};
}
} // namespace

UiPanel::UiPanel() = default;
UiPanel::~UiPanel() = default;

void UiPanel::Initialize(BitmapFont *font) {
  font_ = font;
  rects_.Initialize();
}

void UiPanel::SetBounds(const Vector2 &position, const Vector2 &size) {
  position_ = position;
  size_ = size;
}

void UiPanel::SetTitle(const std::string &title) { title_ = title; }

void UiPanel::Open(float delaySeconds) {
  opened_ = true;
  startTime_ = std::chrono::steady_clock::now() +
               std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                   std::chrono::duration<float>(delaySeconds));
}

float UiPanel::GetProgress_() const {
  if (!opened_) return 0.0f;
  // 待ち時間の間は startTime_ が未来なので経過が負になり、0 に切り上げられる
  const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - startTime_;
  return std::clamp(elapsed.count() / kOpenDurationSec, 0.0f, 1.0f);
}

float UiPanel::GetContentAlpha() const {
  const float progress = GetProgress_();
  const float ramp = std::clamp((progress - kContentStartProgress) / (1.0f - kContentStartProgress), 0.0f, 1.0f);
  return ramp * opacityScale_;
}

bool UiPanel::IsOpened() const { return opened_ && GetProgress_() >= 1.0f; }

float UiPanel::GetHeightForBody(float bodyHeight, float scale) {
  return (kTitleBarHeight + kAccentThickness + kBodyPadding * 2.0f) * scale + bodyHeight;
}

Vector2 UiPanel::GetBodyOrigin() const {
  return {position_.x + kBodyPadding * scale_,
          position_.y + (kTitleBarHeight + kAccentThickness + kBodyPadding) * scale_};
}

Vector2 UiPanel::GetBodySize() const {
  const Vector2 origin = GetBodyOrigin();
  return {position_.x + size_.x - kBodyPadding * scale_ - origin.x, position_.y + size_.y - kBodyPadding * scale_ - origin.y};
}

void UiPanel::Draw() {
  if (!opened_) return;
  // 待ち時間の間は何も描かない(進み具合が 0 のときに最小の線だけが見えてしまうのを防ぐ)
  if (std::chrono::steady_clock::now() < startTime_) return;

  const float openness = (std::max)(EaseOutCubic(GetProgress_()), kMinOpenness);
  DrawFrame_(openness);
  const float contentAlpha = GetContentAlpha();
  if (contentAlpha > 0.0f) DrawDecoration_(contentAlpha);
}

void UiPanel::DrawFrame_(float openness) {
  // 中心の高さを保ったまま、高さだけを openness 倍にして上下に開く
  const float height = size_.y * openness;
  const float top = position_.y + (size_.y - height) * kHalf;
  const float fade = openness * opacityScale_; // 開き始めは薄く、開くほど濃く。全体の倍率も掛ける
  const float borderThickness = ScaledThickness(kBorderThickness, scale_);

  rects_.Fill(UiRect{position_.x, top, size_.x, height}, WithAlphaScale(kBackgroundColor, fade));

  // 枠: 上、下、左、右の 4 本を 1 回の描画命令で
  const UiRect border[] = {
      {position_.x, top, size_.x, borderThickness},
      {position_.x, top + height - borderThickness, size_.x, borderThickness},
      {position_.x, top, borderThickness, height},
      {position_.x + size_.x - borderThickness, top, borderThickness, height},
  };
  rects_.Fill(border, std::size(border), WithAlphaScale(kBorderColor, fade));
}

void UiPanel::DrawDecoration_(float contentAlpha) {
  // 倍率を掛けた寸法(線の太さは、細くなりすぎて消えないよう下限を持つ)
  const float borderThickness = ScaledThickness(kBorderThickness, scale_);
  const float accentThickness = ScaledThickness(kAccentThickness, scale_);
  const float cornerThickness = ScaledThickness(kCornerThickness, scale_);
  const float titleBarHeight = kTitleBarHeight * scale_;
  const float cornerLength = kCornerLength * scale_;

  // タイトルバーと、その下の明るい線
  const UiRect titleBar{position_.x + borderThickness, position_.y + borderThickness,
                        size_.x - borderThickness * 2.0f, titleBarHeight - borderThickness};
  rects_.Fill(titleBar, WithAlphaScale(kTitleBarColor, contentAlpha));
  const UiRect accent{position_.x + borderThickness, position_.y + titleBarHeight,
                      size_.x - borderThickness * 2.0f, accentThickness};
  rects_.Fill(accent, WithAlphaScale(kAccentColor, contentAlpha));

  // 四隅の飾り(L 字を 8 本の長方形で。1 回の描画命令)
  const float right = position_.x + size_.x;
  const float bottom = position_.y + size_.y;
  const UiRect corners[] = {
      {position_.x, position_.y, cornerLength, cornerThickness},
      {position_.x, position_.y, cornerThickness, cornerLength},
      {right - cornerLength, position_.y, cornerLength, cornerThickness},
      {right - cornerThickness, position_.y, cornerThickness, cornerLength},
      {position_.x, bottom - cornerThickness, cornerLength, cornerThickness},
      {position_.x, bottom - cornerLength, cornerThickness, cornerLength},
      {right - cornerLength, bottom - cornerThickness, cornerLength, cornerThickness},
      {right - cornerThickness, bottom - cornerLength, cornerThickness, cornerLength},
  };
  rects_.Fill(corners, std::size(corners), WithAlphaScale(kAccentColor, contentAlpha));

  // タイトル文字(タイトルバーの縦の中央に置く)
  if (font_ && !title_.empty()) {
    const float titleFontSize = kTitleFontSize * scale_;
    const float textHeight = font_->GetLineHeight(titleFontSize);
    const Vector2 textPos{position_.x + kTitlePaddingX * scale_,
                          position_.y + (titleBarHeight - textHeight) * kHalf};
    font_->Draw(title_, textPos, titleFontSize, WithAlphaScale(kTitleTextColor, contentAlpha));
  }
}
