#include "UiMenuList.h"
#include "BitmapFont.h"
#include <algorithm>
#include <cmath>

namespace {
// --- 寸法(px) ---
constexpr float kRowHeight = 56.0f;       // 1 行の高さ
constexpr float kRowGap = 6.0f;           // 行どうしの間隔(ハイライトの帯の間)
constexpr float kFontSize = 32.0f;        // 項目の文字の大きさ
constexpr float kTextPaddingX = 16.0f;    // 帯の左端から選択印までの余白
constexpr float kMarkerWidth = 40.0f;     // 選択印の幅(文字の開始位置をそろえる)
constexpr float kRightPaddingX = 16.0f;   // 帯の右端からロック表示までの余白
constexpr float kLockIconGap = 8.0f;      // ロックの鍵と文字の間隔
constexpr float kHalf = 0.5f;

// --- 点滅と拒否の光 ---
constexpr float kBlinkPeriodSec = 1.2f;   // 点滅の 1 周期
constexpr float kBlinkMinRatio = 0.55f;   // 点滅でいちばん暗いときの、帯の明るさの割合
constexpr float kRejectFlashSec = 0.30f;  // ロック項目を拒んだあとの赤い光の長さ
constexpr float kTwoPi = 6.28318530718f;

// --- 文字列 ---
constexpr const char *kSelectMarker = "\xE2\x96\xB6"; // U+25B6(右向きの三角)
constexpr const char *kLockIcon = "\xEE\x80\x80";     // U+E000(フォントのアトラスに焼いた鍵の絵)
constexpr const char *kLockedLabel = "\xE3\x83\xAD\xE3\x83\x83\xE3\x82\xAF\xE4\xB8\xAD"; // 「ロック中」

// --- 色(RGBA) ---
const Vector4 kBarColor{0.28f, 0.55f, 0.80f, 0.55f};        // 選択中の帯(いちばん明るいときの色)
const Vector4 kBarLockedColor{0.35f, 0.40f, 0.48f, 0.40f};  // ロック中の項目を選んだときの帯(青くしない)
const Vector4 kRejectColor{0.90f, 0.25f, 0.25f, 0.75f};     // 拒んだときに帯が向かう赤
const Vector4 kSelectedTextColor{1.00f, 1.00f, 1.00f, 1.00f};
const Vector4 kNormalTextColor{0.66f, 0.76f, 0.86f, 1.00f};
const Vector4 kLockedTextColor{0.45f, 0.52f, 0.60f, 1.00f};
const Vector4 kMarkerColor{0.62f, 0.86f, 1.00f, 1.00f};

/// 色のアルファに倍率を掛けた色を返す
Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }

/// 2 色を t(0 から 1)で混ぜる
Vector4 Lerp(const Vector4 &a, const Vector4 &b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

/// 2 つの時刻の差(秒)
float SecondsBetween(std::chrono::steady_clock::time_point from, std::chrono::steady_clock::time_point to) {
  return std::chrono::duration<float>(to - from).count();
}
} // namespace

UiMenuList::UiMenuList() = default;
UiMenuList::~UiMenuList() = default;

void UiMenuList::Initialize(BitmapFont *font) {
  font_ = font;
  rects_.Initialize();
  lastMoveTime_ = std::chrono::steady_clock::now();
}

void UiMenuList::SetItems(std::vector<Item> items) {
  items_ = std::move(items);
  cursor_ = 0;
  lastMoveTime_ = std::chrono::steady_clock::now();
}

void UiMenuList::SetItemLabel(int index, const std::string &label) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  items_[index].label = label;
}

void UiMenuList::SetItemLocked(int index, bool locked) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  items_[index].locked = locked;
}

void UiMenuList::SetCursor(int index) {
  if (index < 0 || index >= GetItemCount()) return;
  cursor_ = index;
  lastMoveTime_ = std::chrono::steady_clock::now();
}

float UiMenuList::GetContentHeight() const {
  if (items_.empty()) return 0.0f;
  const float count = static_cast<float>(items_.size());
  return (kRowHeight * count + kRowGap * (count - 1.0f)) * scale_; // 最後の行の下には間隔を入れない
}

const UiMenuList::Item *UiMenuList::GetSelectedItem() const {
  return items_.empty() ? nullptr : &items_[cursor_];
}

UiMenuList::Event UiMenuList::Update(const Input *input) {
  // キーボードとパッドを先に見る。何も押されていなければ、マウスを見る
  const Event keyEvent = Apply(UiMenuInput::Poll(input));
  if (keyEvent != Event::None) return keyEvent;
  return UpdatePointer_(UiPointer::Poll(input));
}

UiMenuList::Event UiMenuList::UpdatePointer_(const UiPointer::State &pointer) {
  if (!pointer.valid || !hasLayout_ || items_.empty()) return Event::None;

  // マウスの下にある項目を探す
  int hovered = -1;
  for (int i = 0; i < GetItemCount(); ++i) {
    const UiRect row{drawOrigin_.x, drawOrigin_.y + drawRowStep_ * static_cast<float>(i), drawWidth_, drawRowHeight_};
    if (UiPointer::Contains(row, pointer.position)) {
      hovered = i;
      break;
    }
  }
  if (hovered < 0) return Event::None;

  // マウスが動いた(または、クリックした)ときだけ、選択を移す。止まったままのマウスが、キーボードの選択を上書きしないため
  bool changed = false;
  if ((pointer.moved || pointer.leftClicked) && hovered != cursor_) {
    SetCursor(hovered);
    changed = true;
  }
  if (pointer.leftClicked) return Apply(UiMenuInput::Command::Decide);
  return changed ? Event::Moved : Event::None;
}

UiMenuList::Event UiMenuList::Apply(UiMenuInput::Command command) {
  if (items_.empty()) return Event::None;

  switch (command) {
  case UiMenuInput::Command::Up:
    MoveCursor_(-1);
    return Event::Moved;
  case UiMenuInput::Command::Down:
    MoveCursor_(1);
    return Event::Moved;
  case UiMenuInput::Command::Decide:
    if (items_[cursor_].locked) {
      lastRejectTime_ = std::chrono::steady_clock::now();
      hasRejected_ = true;
      return Event::DecidedLocked;
    }
    return Event::Decided;
  case UiMenuInput::Command::Left:
  case UiMenuInput::Command::Right:
  case UiMenuInput::Command::None:
    break; // 上下だけのメニューなので、左右は無視する
  }
  return Event::None;
}

void UiMenuList::MoveCursor_(int step) {
  const int count = GetItemCount();
  // 負の移動でも 0 未満にならないよう、count を足してから余りを取る
  cursor_ = (cursor_ + step % count + count) % count;
  lastMoveTime_ = std::chrono::steady_clock::now();
}

float UiMenuList::GetBlinkRatio_() const {
  // 選択を動かした時刻を山の頂点(cos = 1)にして、周期的に暗くなり、また明るくなる
  const float t = SecondsBetween(lastMoveTime_, std::chrono::steady_clock::now());
  const float wave = kHalf + kHalf * std::cos(kTwoPi * t / kBlinkPeriodSec); // 1 から 0 の間を往復
  return kBlinkMinRatio + (1.0f - kBlinkMinRatio) * wave;
}

float UiMenuList::GetRejectFlash_() const {
  if (!hasRejected_) return 0.0f;
  const float t = SecondsBetween(lastRejectTime_, std::chrono::steady_clock::now());
  return std::clamp(1.0f - t / kRejectFlashSec, 0.0f, 1.0f);
}

void UiMenuList::Draw(const Vector2 &origin, float width, float alpha) {
  if (!font_ || alpha <= 0.0f || items_.empty()) return;

  // 寸法は、倍率(scale_)を掛けた値を使う
  const float rowHeight = kRowHeight * scale_;
  const float fontSize = kFontSize * scale_;
  const float textPaddingX = kTextPaddingX * scale_;
  const float markerWidth = kMarkerWidth * scale_;
  const float rightPaddingX = kRightPaddingX * scale_;
  const float lockIconGap = kLockIconGap * scale_;
  const float lineHeight = font_->GetLineHeight(fontSize);
  const float rowStep = rowHeight + kRowGap * scale_;

  // マウスの当たり判定のために、描いた位置を覚える
  hasLayout_ = true;
  drawOrigin_ = origin;
  drawWidth_ = width;
  drawRowHeight_ = rowHeight;
  drawRowStep_ = rowStep;

  for (int i = 0; i < GetItemCount(); ++i) {
    const Item &item = items_[i];
    const bool selected = (i == cursor_);
    const float rowTop = origin.y + rowStep * static_cast<float>(i);
    const float textY = rowTop + (rowHeight - lineHeight) * kHalf;

    // 選択中の帯: 点滅する。ロック中の項目は青くせず灰色にし、拒んだ直後は赤く光らせる
    if (selected) {
      Vector4 bar = WithAlphaScale(item.locked ? kBarLockedColor : kBarColor, GetBlinkRatio_());
      bar = Lerp(bar, kRejectColor, GetRejectFlash_());
      rects_.Fill(UiRect{origin.x, rowTop, width, rowHeight}, WithAlphaScale(bar, alpha));
    }

    // 選択印と項目の文字
    const Vector4 &textColor = item.locked ? kLockedTextColor : (selected ? kSelectedTextColor : kNormalTextColor);
    if (selected) {
      font_->Draw(kSelectMarker, {origin.x + textPaddingX, textY}, fontSize, WithAlphaScale(kMarkerColor, alpha));
    }
    font_->Draw(item.label, {origin.x + textPaddingX + markerWidth, textY}, fontSize,
                WithAlphaScale(textColor, alpha));

    // ロック表示: 右端に「鍵 ロック中」
    if (item.locked) {
      const float rightEdge = origin.x + width - rightPaddingX;
      const float labelWidth = font_->Measure(kLockedLabel, fontSize).x;
      const float iconWidth = font_->Measure(kLockIcon, fontSize).x;
      font_->Draw(kLockedLabel, {rightEdge, textY}, fontSize, WithAlphaScale(kLockedTextColor, alpha),
                  BitmapFont::Align::Right);
      font_->Draw(kLockIcon, {rightEdge - labelWidth - lockIconGap - iconWidth, textY}, fontSize,
                  WithAlphaScale(kLockedTextColor, alpha));
    }
  }
}
