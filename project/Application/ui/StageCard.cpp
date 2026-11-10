#include "StageCard.h"
#include "BitmapFont.h"
#include "../stage/StageDefinition.h"
#include <cstdio>

namespace {
// --- 寸法(px) ---
constexpr float kTagFontSize = 20.0f;         // 「STAGE 01」の文字の大きさ
constexpr float kNameFontSize = 48.0f;        // ステージ名の文字の大きさ
constexpr float kDescriptionFontSize = 24.0f; // 説明文の文字の大きさ
constexpr float kTagGap = 4.0f;               // 番号と名前の間隔
constexpr float kRuleGapTop = 12.0f;          // 名前と区切り線の間隔
constexpr float kRuleThickness = 2.0f;        // 区切り線の太さ
constexpr float kRuleGapBottom = 16.0f;       // 区切り線と説明文の間隔
constexpr float kLockIconGap = 10.0f;         // 番号とロックの鍵の間隔

// --- 文字列(画面に出す日本語) ---
constexpr const char *kCardTitle = "ステージ情報";
constexpr const char *kLockedName = "???";
constexpr const char *kLockedDescription = "このステージは、まだ遊べません。\n近日公開予定です。";
constexpr const char *kLockIcon = "\xEE\x80\x80"; // U+E000(フォントのアトラスに焼いた鍵の絵)
constexpr const char *kTagFormat = "STAGE %02d";
constexpr size_t kTagBufferSize = 32;

// --- 色(RGBA) ---
const Vector4 kTagColor{0.62f, 0.86f, 1.00f, 1.00f};         // 番号(明るい水色)
const Vector4 kNameColor{1.00f, 1.00f, 1.00f, 1.00f};        // 名前
const Vector4 kRuleColor{0.62f, 0.86f, 1.00f, 0.50f};        // 区切り線
const Vector4 kDescriptionColor{0.78f, 0.86f, 0.94f, 1.00f}; // 説明文
const Vector4 kLockedColor{0.55f, 0.62f, 0.70f, 1.00f};      // ロック中の文字

/// 色のアルファに倍率を掛けた色を返す
Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }
} // namespace

StageCard::StageCard() = default;
StageCard::~StageCard() = default;

void StageCard::Initialize(BitmapFont *font) {
  font_ = font;
  panel_.Initialize(font);
  panel_.SetTitle(kCardTitle);
  rects_.Initialize();
}

void StageCard::SetBounds(const Vector2 &position, const Vector2 &size) { panel_.SetBounds(position, size); }

void StageCard::Open(float delaySeconds) { panel_.Open(delaySeconds); }

void StageCard::SetStage(const StageDefinition *stage, int number) {
  stage_ = stage;
  number_ = number;
}

void StageCard::Draw() {
  panel_.Draw();
  const float alpha = panel_.GetContentAlpha();
  if (alpha > 0.0f && stage_ && font_) DrawContent_(alpha);
}

void StageCard::DrawContent_(float alpha) {
  const bool locked = !stage_->unlocked;
  const Vector2 origin = panel_.GetBodyOrigin();
  const Vector2 body = panel_.GetBodySize();
  float y = origin.y;

  // 1. 番号(ロック中は、右に鍵を添える)
  char tag[kTagBufferSize];
  std::snprintf(tag, sizeof(tag), kTagFormat, number_);
  const Vector4 tagColor = locked ? kLockedColor : kTagColor;
  font_->Draw(tag, {origin.x, y}, kTagFontSize, WithAlphaScale(tagColor, alpha));
  if (locked) {
    const float tagWidth = font_->Measure(tag, kTagFontSize).x;
    font_->Draw(kLockIcon, {origin.x + tagWidth + kLockIconGap, y}, kTagFontSize, WithAlphaScale(kLockedColor, alpha));
  }
  y += font_->GetLineHeight(kTagFontSize) + kTagGap;

  // 2. 名前(ロック中は「???」)
  font_->Draw(locked ? kLockedName : stage_->displayName, {origin.x, y}, kNameFontSize,
              WithAlphaScale(locked ? kLockedColor : kNameColor, alpha));
  y += font_->GetLineHeight(kNameFontSize) + kRuleGapTop;

  // 3. 区切り線
  rects_.Fill(UiRect{origin.x, y, body.x, kRuleThickness}, WithAlphaScale(kRuleColor, alpha));
  y += kRuleThickness + kRuleGapBottom;

  // 4. 説明文(ロック中は、遊べないことだけを伝える)
  font_->Draw(locked ? kLockedDescription : stage_->description, {origin.x, y}, kDescriptionFontSize,
              WithAlphaScale(locked ? kLockedColor : kDescriptionColor, alpha));
}
