#include "StageCard.h"
#include "BitmapFont.h"
#include "../stage/StageDefinition.h"
#include "../stage/StageProgress.h"
#include <algorithm>
#include <cmath>
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

// --- 鍵が外れる演出(解放されたステージを、初めて見るとき 1 回だけ) ---
// 時刻(秒。演出の始まりからの経過)。震える -> 鍵が外れて落ちる -> 名前と説明文が現れる(同時に、カードが一瞬光る)
constexpr float kUnlockShakeSeconds = 0.50f;       // 鍵が震える時間(外れる直前まで)
constexpr float kUnlockFallStartSeconds = 0.50f;   // 鍵が外れて、落ち始める時刻
constexpr float kUnlockFallSeconds = 0.40f;        // 鍵が落ちて、消えるまでの時間
constexpr float kUnlockRevealStartSeconds = 0.55f; // 名前と説明文が、現れ始める時刻(鍵が外れた直後)
constexpr float kUnlockRevealFadeSeconds = 0.40f;  // 名前と説明文が、現れ切るまでの時間
constexpr float kUnlockFlashSeconds = 0.25f;       // 現れ始めの、カードが光る時間
constexpr float kUnlockTotalSeconds = 1.20f;       // 演出全体の長さ(これを過ぎたら、終わり)
constexpr float kUnlockShakeAmplitude = 4.0f;      // 鍵の震えの大きさ(px。震えるうちに、だんだん大きくなる)
constexpr float kUnlockShakeSpeed = 70.0f;         // 鍵の震えの速さ(ラジアン / 秒)
constexpr float kUnlockShakeStartRatio = 0.4f;     // 震え始めの大きさ(最大に対する割合)
constexpr float kUnlockFallDistance = 60.0f;       // 鍵が落ちる距離(px)
constexpr float kUnlockFlashAlpha = 0.45f;         // カードが光る、最も明るい濃さ

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
const Vector4 kUnlockFlashColor{1.00f, 0.95f, 0.75f, 1.00f}; // 鍵が外れたときの、カードの光(白に近い金)

/// 色のアルファに倍率を掛けた色を返す
Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }

float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }
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
  unlockState_ = UnlockState::None; // 別のステージを表示するので、鍵の演出は、いったん無し(必要なら、呼び出し側が、あらためて始める)
}

void StageCard::PlayUnlockAnimation() {
  unlockState_ = UnlockState::Waiting;
}

bool StageCard::IsUnlockAnimationFinished() const {
  return unlockState_ == UnlockState::Finished;
}

void StageCard::Draw() {
  panel_.Draw();
  const float alpha = panel_.GetContentAlpha();
  UpdateUnlockState_(alpha);
  if (alpha > 0.0f && stage_ && font_) DrawContent_(alpha);
}

void StageCard::UpdateUnlockState_(float contentAlpha) {
  // 遷移の覆いが晴れて(unlockAllowed_)、中身がすっかり現れてから、演出を始める(覆いの最中や、現れる途中に、始めてしまわないため)
  if (unlockState_ == UnlockState::Waiting && unlockAllowed_ && contentAlpha >= 1.0f) {
    unlockState_ = UnlockState::Playing;
    unlockStart_ = std::chrono::steady_clock::now();
  }
  if (unlockState_ == UnlockState::Playing && GetUnlockElapsed_() >= kUnlockTotalSeconds) {
    unlockState_ = UnlockState::Finished;
  }
}

float StageCard::GetUnlockElapsed_() const {
  return std::chrono::duration<float>(std::chrono::steady_clock::now() - unlockStart_).count();
}

void StageCard::DrawContent_(float alpha) {
  const bool unlocked = StageProgress::GetInstance().IsUnlocked(*stage_);
  // 鍵の演出の最中(始まるのを待つ間を含む)は、解放済みでも、解放の瞬間まで、ロック中の表示のまま
  const bool animating = unlocked && (unlockState_ == UnlockState::Waiting || unlockState_ == UnlockState::Playing);
  const float t = unlockState_ == UnlockState::Playing ? GetUnlockElapsed_() : 0.0f;
  // 名前と説明文が現れた割合(0 = まだ「???」、1 = 現れ切った)。演出が無いときは、解放済みなら 1、ロック中なら 0
  float revealRatio = unlocked ? 1.0f : 0.0f;
  if (animating) {
    revealRatio = unlockState_ == UnlockState::Playing ? Saturate((t - kUnlockRevealStartSeconds) / kUnlockRevealFadeSeconds) : 0.0f;
  }
  const bool showLocked = revealRatio <= 0.0f; // 名前と説明文を、ロック中の文言にするか

  const Vector2 origin = panel_.GetBodyOrigin();
  const Vector2 body = panel_.GetBodySize();
  float y = origin.y;

  // 1. 番号(ロック中は、右に鍵を添える)
  char tag[kTagBufferSize];
  std::snprintf(tag, sizeof(tag), kTagFormat, number_);
  const Vector4 tagColor = showLocked ? kLockedColor : kTagColor;
  font_->Draw(tag, {origin.x, y}, kTagFontSize, WithAlphaScale(tagColor, alpha));
  const float tagWidth = font_->Measure(tag, kTagFontSize).x;
  if (!unlocked || (animating && unlockState_ == UnlockState::Waiting)) {
    // ロック中、または、演出の始まりを待っている間: 鍵を、そのまま出す
    font_->Draw(kLockIcon, {origin.x + tagWidth + kLockIconGap, y}, kTagFontSize, WithAlphaScale(kLockedColor, alpha));
  } else if (animating) {
    // 演出の最中: 震えて、外れて、落ちて、消える
    float iconX = origin.x + tagWidth + kLockIconGap;
    float iconY = y;
    float iconAlpha = alpha;
    if (t < kUnlockFallStartSeconds) {
      const float shakeT = Saturate(t / kUnlockShakeSeconds);
      const float amplitude = kUnlockShakeAmplitude * (kUnlockShakeStartRatio + (1.0f - kUnlockShakeStartRatio) * shakeT);
      iconX += std::sin(t * kUnlockShakeSpeed) * amplitude;
    } else {
      const float fallT = Saturate((t - kUnlockFallStartSeconds) / kUnlockFallSeconds);
      iconY += kUnlockFallDistance * fallT * fallT; // 加速しながら落ちる
      iconAlpha *= 1.0f - fallT;
    }
    if (iconAlpha > 0.0f) {
      font_->Draw(kLockIcon, {iconX, iconY}, kTagFontSize, WithAlphaScale(kLockedColor, iconAlpha));
    }
  }
  y += font_->GetLineHeight(kTagFontSize) + kTagGap;

  // 2. 名前(ロック中は「???」。現れ始めたら、名前が、だんだん濃くなる)
  font_->Draw(showLocked ? kLockedName : stage_->displayName, {origin.x, y}, kNameFontSize,
              WithAlphaScale(showLocked ? kLockedColor : kNameColor, alpha * (showLocked ? 1.0f : revealRatio)));
  y += font_->GetLineHeight(kNameFontSize) + kRuleGapTop;

  // 3. 区切り線
  rects_.Fill(UiRect{origin.x, y, body.x, kRuleThickness}, WithAlphaScale(kRuleColor, alpha));
  y += kRuleThickness + kRuleGapBottom;

  // 4. 説明文(ロック中は、遊べないことだけを伝える)
  font_->Draw(showLocked ? kLockedDescription : stage_->description, {origin.x, y}, kDescriptionFontSize,
              WithAlphaScale(showLocked ? kLockedColor : kDescriptionColor, alpha * (showLocked ? 1.0f : revealRatio)));

  // 5. 鍵が外れた瞬間の、カードの光(現れ始めから、すぐ収まる)
  if (animating && unlockState_ == UnlockState::Playing) {
    const float flashT = Saturate((t - kUnlockRevealStartSeconds) / kUnlockFlashSeconds);
    const float flashAlpha = kUnlockFlashAlpha * (1.0f - flashT) * (t >= kUnlockRevealStartSeconds ? 1.0f : 0.0f);
    if (flashAlpha > 0.0f) {
      rects_.Fill(UiRect{origin.x, origin.y, body.x, body.y}, WithAlphaScale(kUnlockFlashColor, flashAlpha * alpha));
    }
  }
}
