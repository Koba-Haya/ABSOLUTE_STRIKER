#include "InheritancePicker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "UiMenuInput.h"
#include "../ScreenSize.h"
#include "../item/ItemCatalog.h"
#include "../item/PlayerUpgradeState.h"
#include "../result/RatingCatalog.h"

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

// --- 配置(px。ResultTally と同じ左右のパネルの配置) ---
constexpr float kListPanelLeft = 90.0f;
constexpr float kListPanelWidth = 700.0f;
constexpr float kPanelGap = 20.0f;
constexpr float kRightMargin = 90.0f;
constexpr float kHalf = 0.5f;

// --- 左のパネル: 行の寸法と、列の位置(本体領域の左端から) ---
constexpr float kMaxRowHeight = 44.0f;       // 1 行の高さの上限(行が少ないとき、間延びさせない)
constexpr float kRowFontRatio = 0.55f;       // 文字の大きさ(行の高さに対する割合)
constexpr float kMaxRowFontSize = 22.0f;     // 行の文字の大きさの上限
constexpr float kHeaderFontSize = 14.0f;     // 列の見出しの文字の大きさ
constexpr float kRowPadX = 12.0f;            // 行の左の余白
constexpr float kSwatchSize = 14.0f;         // 強化の色の印(四角)の大きさ
constexpr float kSwatchGap = 10.0f;          // 印と名前の間隔
constexpr float kStageColumnX = 250.0f;      // 強化の段階(枠の並び)の列の左端
constexpr float kPickColumnX = 470.0f;       // 取った数(枠の並び)の列の左端
constexpr float kArrowGap = 8.0f;            // 取った数の枠の並びと、左右の矢印の間隔
constexpr float kRowInset = 2.0f;            // 選択中の行の強調の、上下の内側への余白
constexpr float kCursorBarWidth = 3.0f;      // 選択中の行の、左端の印の幅
constexpr float kPipHeight = 12.0f;          // 枠の高さ(ゲーム中のロックオン数と同じ)
constexpr float kStagePipWidth = 26.0f;      // 強化の段階の枠の幅
constexpr float kPickPipWidth = 18.0f;       // 取った数の枠の幅
constexpr float kPipGap = 4.0f;              // 枠と枠の間隔

// --- 右のパネル ---
constexpr float kInfoFontSize = 24.0f; // 右のパネルの文字の大きさ
constexpr float kInfoLineGap = 10.0f;  // 右のパネルの行の間隔
constexpr float kRankFontSize = 34.0f; // ランクの文字の大きさ
constexpr float kSlotPipWidth = 28.0f; // 継承の枠の並びの、1 つの枠の幅
constexpr float kMessageSeconds = 1.2f; // 「枠がいっぱいです」を出す時間

// --- 点滅 ---
constexpr float kBlinkPeriodSeconds = 1.2f; // 選択中の行と、増える分の点滅の 1 周期
constexpr float kTwoPi = 6.28318530718f;
constexpr float kRowHighlightBase = 0.06f;  // 選択中の行の背景の濃さ(いちばん薄いとき)
constexpr float kRowHighlightRange = 0.10f; // 点滅で足される濃さ
constexpr float kGainBlinkMin = 0.45f;      // 増える分の、いちばん薄いときの濃さ

// --- 文字列(画面に出す日本語) ---
constexpr const char *kListTitle = "継承の選択";
constexpr const char *kInfoTitle = "継承の枠";
constexpr const char *kStageHeader = "強化の段階";
constexpr const char *kPickHeader = "取った強化(選ぶ)";
constexpr const char *kArrowLeft = "<";
constexpr const char *kArrowRight = ">";
constexpr const char *kConfirmLabel = "この内容で継承する";
constexpr const char *kConfirmNoneLabel = "継承しない";
constexpr const char *kFullMessage = "枠がいっぱいです";
constexpr const char *kDoneMessage = "継承しました";
constexpr const char *kSlotsCaption = "使う枠";
constexpr size_t kTextBufferSize = 128;

// --- 色(RGBA) ---
const Vector4 kInfoColor{1.00f, 1.00f, 1.00f, 1.00f};
const Vector4 kInfoLabelColor{0.78f, 0.86f, 0.94f, 1.00f};
const Vector4 kGoldColor{1.00f, 0.84f, 0.30f, 1.00f};
const Vector4 kGainColor{1.00f, 0.95f, 0.65f, 1.00f}; // これから増える分(明るい金。継承済みの色と見分ける)
const Vector4 kWarnColor{1.00f, 0.45f, 0.40f, 1.00f};
const Vector4 kRowHighlightColor{1.00f, 1.00f, 1.00f, 1.00f};
const Vector4 kRankColorS{1.00f, 0.84f, 0.30f, 1.00f};
const Vector4 kRankColorA{1.00f, 0.55f, 0.25f, 1.00f};
const Vector4 kRankColorB{0.45f, 0.78f, 1.00f, 1.00f};
const Vector4 kRankColorC{0.70f, 0.74f, 0.80f, 1.00f};

Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }

const Vector4 &RankColor(RunRank rank) {
  switch (rank) {
  case RunRank::S: return kRankColorS;
  case RunRank::A: return kRankColorA;
  case RunRank::B: return kRankColorB;
  case RunRank::C: return kRankColorC;
  }
  return kRankColorC;
}

} // namespace

const ItemDefinition *InheritancePicker::FindDefinition_(ItemEffectKind kind) const {
  for (const ItemDefinition &item : items_->GetItems()) {
    if (item.effect == kind) return &item;
  }
  return nullptr;
}

bool InheritancePicker::HasCandidates() {
  const PlayerUpgradeState &state = PlayerUpgradeState::Persistent();
  for (size_t i = 0; i < kItemEffectKindCount; ++i) {
    if (state.GetEarned(static_cast<ItemEffectKind>(i)) > 0) return true;
  }
  return false;
}

void InheritancePicker::Initialize(const ItemCatalog &items, const RatingCatalog &rating, const InheritanceState &inheritance) {
  items_ = &items;
  rating_ = &rating;
  inheritance_ = &inheritance;
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);
  listPanel_.Initialize(&font_);
  listPanel_.SetTitle(kListTitle);
  infoPanel_.Initialize(&font_);
  infoPanel_.SetTitle(kInfoTitle);
  rects_.Initialize();
  pips_.Initialize();
}

void InheritancePicker::SetVerticalRange(float top, float bottom) {
  top_ = top;
  bottom_ = bottom;
}

int InheritancePicker::UsedSlots_() const {
  int used = 0;
  for (const Candidate &candidate : candidates_) used += candidate.picked;
  return used;
}

void InheritancePicker::Begin(RunRank rank, float delaySeconds) {
  const PlayerUpgradeState &state = PlayerUpgradeState::Persistent();
  const ItemUpgradeSettings &upgrade = items_->GetUpgradeSettings();
  const InheritanceRule &rule = rating_->GetSettings().GetInheritanceRule(rank);

  // 候補: 今回取った強化の種類(取った数が 1 以上のもの)
  candidates_.clear();
  for (size_t i = 0; i < kItemEffectKindCount; ++i) {
    const ItemEffectKind kind = static_cast<ItemEffectKind>(i);
    const int earned = state.GetEarned(kind);
    if (earned <= 0) continue;
    Candidate candidate;
    candidate.kind = kind;
    candidate.earned = earned;
    candidate.maxStacks = upgrade.Get(kind).maxStacks;
    candidate.inherited = inheritance_->GetInheritedStacks(kind);
    if (const ItemDefinition *definition = FindDefinition_(kind)) {
      candidate.name = definition->shortName;
      candidate.color = {definition->uiColor.x, definition->uiColor.y, definition->uiColor.z, 1.0f};
    } else {
      candidate.name = ItemCatalog::GetEffectName(kind);
    }
    candidates_.push_back(std::move(candidate));
  }
  if (candidates_.empty()) return;

  rank_ = rank;
  slots_ = rule.slots;
  rarity_ = rule.rarity;
  active_ = true;
  done_ = false;
  hasMessage_ = false;
  cursor_ = 0;
  moveTime_ = std::chrono::steady_clock::now();

  // 配置: 左右のパネルを並べる(ResultTally と同じ位置)
  const float height = (std::max)(bottom_ - top_, 0.0f);
  const float rightLeft = kListPanelLeft + kListPanelWidth + kPanelGap;
  const float rightWidth = ScreenSize::kWidth - kRightMargin - rightLeft;
  listPanel_.SetBounds({kListPanelLeft, top_}, {kListPanelWidth, height});
  infoPanel_.SetBounds({rightLeft, top_}, {rightWidth, height});
  listPanel_.Open(delaySeconds);
  infoPanel_.Open(delaySeconds);
}

std::vector<InheritanceState::Pick> InheritancePicker::GetPicks() const {
  std::vector<InheritanceState::Pick> picks;
  for (const Candidate &candidate : candidates_) {
    if (candidate.picked > 0) picks.push_back({candidate.kind, candidate.picked});
  }
  return picks;
}

void InheritancePicker::ChangePicked_(Candidate &candidate, int delta) {
  if (delta > 0) {
    if (candidate.picked >= candidate.earned) return; // 取った数より多くは選べない
    if (UsedSlots_() >= slots_) {
      hasMessage_ = true; // 枠がいっぱいで、増やせない
      messageTime_ = std::chrono::steady_clock::now();
      return;
    }
    ++candidate.picked;
  } else if (delta < 0 && candidate.picked > 0) {
    --candidate.picked;
  }
}

void InheritancePicker::Decide_() {
  if (cursor_ >= static_cast<int>(candidates_.size())) {
    done_ = true; // 最後の行: 確定
    return;
  }
  Candidate &candidate = candidates_[static_cast<size_t>(cursor_)];
  // 決定だけで操作できるように(パッドの A ボタンだけなど)、取った数まで選んでいたら、0 へ戻して、また選び直せるようにする
  if (candidate.picked >= candidate.earned) {
    candidate.picked = 0;
  } else {
    ChangePicked_(candidate, 1);
  }
}

float InheritancePicker::GetBlink_() const {
  // 行を動かした直後がいちばん明るい(動かしたことが見て分かるように)。cos で 1 から 0 を往復する
  const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - moveTime_).count();
  return kHalf + kHalf * std::cos(kTwoPi * elapsed / kBlinkPeriodSeconds);
}

void InheritancePicker::Update(const Input *input, bool acceptInput) {
  if (!active_ || done_ || !acceptInput || !listPanel_.IsOpened()) return;

  const UiMenuInput::Command command = UiMenuInput::Poll(input);
  // キーボードとパッドの入力が無ければ、マウスを見る
  if (command == UiMenuInput::Command::None) {
    UpdatePointer_(UiPointer::Poll(input));
    return;
  }

  const int rowCount = static_cast<int>(candidates_.size()) + 1; // 候補の行と、確定の行
  const bool onCandidate = cursor_ < static_cast<int>(candidates_.size());
  switch (command) {
  case UiMenuInput::Command::Up:
    cursor_ = (cursor_ - 1 + rowCount) % rowCount;
    moveTime_ = std::chrono::steady_clock::now();
    break;
  case UiMenuInput::Command::Down:
    cursor_ = (cursor_ + 1) % rowCount;
    moveTime_ = std::chrono::steady_clock::now();
    break;
  case UiMenuInput::Command::Left:
    if (onCandidate) ChangePicked_(candidates_[static_cast<size_t>(cursor_)], -1);
    break;
  case UiMenuInput::Command::Right:
    if (onCandidate) ChangePicked_(candidates_[static_cast<size_t>(cursor_)], 1);
    break;
  case UiMenuInput::Command::Decide:
    Decide_();
    break;
  case UiMenuInput::Command::None:
    break;
  }
}

InheritancePicker::ListGeometry InheritancePicker::GetListGeometry_() const {
  ListGeometry geometry;
  geometry.origin = listPanel_.GetBodyOrigin();
  geometry.body = listPanel_.GetBodySize();
  // 行の高さ: 見出しの行、候補の行、確定の行を、本体の高さに収める(多くても、上限まで)
  const int lineCount = static_cast<int>(candidates_.size()) + 2;
  geometry.rowHeight = (std::min)(geometry.body.y / static_cast<float>(lineCount), kMaxRowHeight);
  geometry.fontSize = (std::min)(geometry.rowHeight * kRowFontRatio, kMaxRowFontSize);
  geometry.rowsTop = geometry.origin.y + geometry.rowHeight;
  return geometry;
}

UiRect InheritancePicker::GetArrowRect_(const ListGeometry &geometry, size_t index, bool right) const {
  const int earned = candidates_[index].earned;
  const float arrowWidth = font_.Measure(right ? kArrowRight : kArrowLeft, geometry.fontSize).x;
  const float hitWidth = arrowWidth + kArrowGap * 2.0f; // 矢印の文字より、少し広く取る(押しやすいように)
  const float pickWidth = kPickPipWidth * static_cast<float>(earned) + kPipGap * static_cast<float>(earned - 1);
  const float x = right ? geometry.origin.x + kPickColumnX + pickWidth : geometry.origin.x + kPickColumnX - hitWidth;
  return UiRect{x, geometry.rowsTop + geometry.rowHeight * static_cast<float>(index), hitWidth, geometry.rowHeight};
}

void InheritancePicker::UpdatePointer_(const UiPointer::State &pointer) {
  if (!pointer.valid) return;
  const ListGeometry geometry = GetListGeometry_();

  // マウスの下にある行を探す(候補の行と、確定の行)
  const size_t rowCount = candidates_.size() + 1;
  int hovered = -1;
  for (size_t i = 0; i < rowCount; ++i) {
    const UiRect row{geometry.origin.x, geometry.rowsTop + geometry.rowHeight * static_cast<float>(i), geometry.body.x, geometry.rowHeight};
    if (UiPointer::Contains(row, pointer.position)) {
      hovered = static_cast<int>(i);
      break;
    }
  }
  if (hovered < 0) return;

  // マウスが動いた(または、クリックした)ときだけ、選択を移す。止まったままのマウスが、キーボードの選択を上書きしないため
  if ((pointer.moved || pointer.leftClicked || pointer.rightClicked) && hovered != cursor_) {
    cursor_ = hovered;
    moveTime_ = std::chrono::steady_clock::now();
  }
  const bool onCandidate = hovered < static_cast<int>(candidates_.size());
  if (pointer.rightClicked && onCandidate) {
    ChangePicked_(candidates_[static_cast<size_t>(hovered)], -1); // 右クリック: 1 個減らす
    return;
  }
  if (!pointer.leftClicked) return;
  if (!onCandidate) {
    done_ = true; // 確定の行
    return;
  }
  // 左クリック: 左の矢印なら減らし、右の矢印なら増やし、それ以外の場所なら、決定と同じ(1 個増やす。取った数まで選んでいたら 0 へ戻る)
  const size_t index = static_cast<size_t>(hovered);
  if (UiPointer::Contains(GetArrowRect_(geometry, index, false), pointer.position)) {
    ChangePicked_(candidates_[index], -1);
  } else if (UiPointer::Contains(GetArrowRect_(geometry, index, true), pointer.position)) {
    ChangePicked_(candidates_[index], 1);
  } else {
    Decide_();
  }
}

void InheritancePicker::BeginAutoPlay(float stepSeconds, int maxPicksPerRow) {
  autoPlay_ = true;
  autoStepSeconds_ = stepSeconds;
  autoMaxPicksPerRow_ = maxPicksPerRow;
  autoTimer_ = 0.0f;
}

void InheritancePicker::UpdateAutoPlay(float deltaTime) {
  // パネルが開いて、操作できる状態になってから数える
  if (!autoPlay_ || !active_ || done_ || !listPanel_.IsOpened()) return;
  autoTimer_ += deltaTime;
  if (autoTimer_ < autoStepSeconds_) return;
  autoTimer_ = 0.0f;

  // 人が操作するのと同じ関数を通す(画面の動きが、本物の操作と同じになる)
  if (cursor_ >= static_cast<int>(candidates_.size())) {
    Decide_(); // 最後の行: 確定する
    return;
  }
  Candidate &candidate = candidates_[static_cast<size_t>(cursor_)];
  const int target = (std::min)(candidate.earned, autoMaxPicksPerRow_);
  if (candidate.picked < target && UsedSlots_() < slots_) {
    ChangePicked_(candidate, 1); // 個数を 1 つ増やす
  } else {
    cursor_ = cursor_ + 1; // 次の行へ(最後の候補の次は、確定の行)
    moveTime_ = std::chrono::steady_clock::now();
  }
}

void InheritancePicker::Draw() {
  if (!active_) return;
  listPanel_.Draw();
  infoPanel_.Draw();
  const float listAlpha = listPanel_.GetContentAlpha();
  if (listAlpha > 0.0f) DrawList_(listAlpha);
  const float infoAlpha = infoPanel_.GetContentAlpha();
  if (infoAlpha > 0.0f) DrawInfo_(infoAlpha);
}

void InheritancePicker::DrawList_(float alpha) {
  const ListGeometry geometry = GetListGeometry_();
  const Vector2 origin = geometry.origin;
  const Vector2 body = geometry.body;
  const float rowHeight = geometry.rowHeight;
  const float fontSize = geometry.fontSize;
  const float blink = GetBlink_();
  const float headerTextY = origin.y + (rowHeight - font_.GetLineHeight(kHeaderFontSize)) * kHalf;

  // 列の見出し
  font_.Draw(kStageHeader, {origin.x + kStageColumnX, headerTextY}, kHeaderFontSize, WithAlphaScale(kInfoLabelColor, alpha));
  font_.Draw(kPickHeader, {origin.x + kPickColumnX, headerTextY}, kHeaderFontSize, WithAlphaScale(kInfoLabelColor, alpha));

  // 枠の並びの見た目(強化の段階と、取った数で、枠の幅だけ変える)
  UiPipBar::Style stageStyle;
  stageStyle.pipSize = {kStagePipWidth, kPipHeight};
  stageStyle.gap = kPipGap;
  stageStyle.alpha = alpha;
  stageStyle.gainColor = kGainColor;
  stageStyle.gainBlink = kGainBlinkMin + (1.0f - kGainBlinkMin) * blink;
  UiPipBar::Style pickStyle;
  pickStyle.pipSize = {kPickPipWidth, kPipHeight};
  pickStyle.gap = kPipGap;
  pickStyle.alpha = alpha;
  pickStyle.baseColor = kGoldColor;

  const float rowsTop = geometry.rowsTop;
  for (size_t i = 0; i < candidates_.size(); ++i) {
    const Candidate &candidate = candidates_[i];
    const float rowTop = rowsTop + rowHeight * static_cast<float>(i);
    const bool isCursor = static_cast<int>(i) == cursor_;

    // 選択中の行: 背景を点滅させ、左端に印を付ける
    if (isCursor) {
      const float highlight = kRowHighlightBase + kRowHighlightRange * blink;
      rects_.Fill(UiRect{origin.x, rowTop + kRowInset, body.x, rowHeight - kRowInset * 2.0f}, WithAlphaScale(kRowHighlightColor, highlight * alpha));
      rects_.Fill(UiRect{origin.x, rowTop + kRowInset, kCursorBarWidth, rowHeight - kRowInset * 2.0f}, WithAlphaScale(kGoldColor, alpha));
    }

    // 強化の色の印と、名前
    const float swatchY = rowTop + (rowHeight - kSwatchSize) * kHalf;
    rects_.Fill(UiRect{origin.x + kRowPadX, swatchY, kSwatchSize, kSwatchSize}, WithAlphaScale(candidate.color, alpha));
    const float textY = rowTop + (rowHeight - font_.GetLineHeight(fontSize)) * kHalf;
    font_.Draw(candidate.name, {origin.x + kRowPadX + kSwatchSize + kSwatchGap, textY}, fontSize, WithAlphaScale(kInfoColor, alpha));

    // 強化の段階: 継承済みの分(強化の色)と、選んだ分で増える分(点滅)。上限は枠の数
    const float pipY = rowTop + (rowHeight - kPipHeight) * kHalf;
    stageStyle.baseColor = candidate.color;
    pips_.Draw({origin.x + kStageColumnX, pipY}, candidate.maxStacks, candidate.inherited,
               static_cast<float>(candidate.picked) * rarity_, stageStyle);

    // 取った数: 取った個数だけ枠があり、選んだ個数だけ金色になる。選択中の行には、増減の矢印を添える
    const float pickWidth = UiPipBar::GetWidth(candidate.earned, pickStyle);
    pips_.Draw({origin.x + kPickColumnX, pipY}, candidate.earned, static_cast<float>(candidate.picked), 0.0f, pickStyle);
    if (isCursor) {
      const Vector4 arrowColor = WithAlphaScale(kGoldColor, alpha);
      const float arrowWidth = font_.Measure(kArrowLeft, fontSize).x;
      font_.Draw(kArrowLeft, {origin.x + kPickColumnX - kArrowGap - arrowWidth, textY}, fontSize, arrowColor);
      font_.Draw(kArrowRight, {origin.x + kPickColumnX + pickWidth + kArrowGap, textY}, fontSize, arrowColor);
    }
  }

  // 確定の行
  const size_t confirmIndex = candidates_.size();
  const float confirmTop = rowsTop + rowHeight * static_cast<float>(confirmIndex);
  const bool confirmCursor = cursor_ == static_cast<int>(confirmIndex);
  if (confirmCursor) {
    const float highlight = kRowHighlightBase + kRowHighlightRange * blink;
    rects_.Fill(UiRect{origin.x, confirmTop + kRowInset, body.x, rowHeight - kRowInset * 2.0f}, WithAlphaScale(kRowHighlightColor, highlight * alpha));
  }
  const char *confirmLabel = UsedSlots_() > 0 ? kConfirmLabel : kConfirmNoneLabel;
  const float confirmTextY = confirmTop + (rowHeight - font_.GetLineHeight(fontSize)) * kHalf;
  font_.Draw(confirmLabel, {origin.x + body.x * kHalf, confirmTextY}, fontSize, WithAlphaScale(confirmCursor ? kGoldColor : kInfoColor, alpha),
             BitmapFont::Align::Center);
}

void InheritancePicker::DrawInfo_(float alpha) {
  const Vector2 origin = infoPanel_.GetBodyOrigin();
  const Vector2 body = infoPanel_.GetBodySize();
  const float centerX = origin.x + body.x * kHalf;
  const float lineStep = font_.GetLineHeight(kInfoFontSize) + kInfoLineGap;
  float y = origin.y;
  char text[kTextBufferSize];

  // ランク
  std::snprintf(text, sizeof(text), "ランク  %s", RankToString(rank_));
  font_.Draw(text, {centerX, y}, kRankFontSize, WithAlphaScale(RankColor(rank_), alpha), BitmapFont::Align::Center);
  y += font_.GetLineHeight(kRankFontSize) + kInfoLineGap;

  // 継承の枠: 枠の並び(使った分が埋まる)。数字は出さない
  font_.Draw(kSlotsCaption, {centerX, y}, kInfoFontSize, WithAlphaScale(kInfoLabelColor, alpha), BitmapFont::Align::Center);
  y += lineStep;
  UiPipBar::Style slotStyle;
  slotStyle.pipSize = {kSlotPipWidth, kPipHeight};
  slotStyle.gap = kPipGap;
  slotStyle.alpha = alpha;
  slotStyle.baseColor = kGoldColor;
  const float slotsWidth = UiPipBar::GetWidth(slots_, slotStyle);
  pips_.Draw({centerX - slotsWidth * kHalf, y}, slots_, static_cast<float>(UsedSlots_()), 0.0f, slotStyle);
  y += kPipHeight + kInfoLineGap * 2.0f;

  // 強化の倍率(レアリティ)
  std::snprintf(text, sizeof(text), "強化の倍率  x%.1f", rarity_);
  font_.Draw(text, {centerX, y}, kInfoFontSize, WithAlphaScale(rarity_ > 1.0f ? kGoldColor : kInfoLabelColor, alpha), BitmapFont::Align::Center);
  y += lineStep;

  // 世代(0 世代が、第 1 世代。確定すると、次の世代へ進む)
  const int generation = inheritance_->GetGeneration();
  std::snprintf(text, sizeof(text), "第 %d 世代 -> 第 %d 世代", generation + 1, generation + 2);
  font_.Draw(text, {centerX, y}, kInfoFontSize, WithAlphaScale(kInfoLabelColor, alpha), BitmapFont::Align::Center);
  y += lineStep;

  // 知らせ: 枠がいっぱいのとき(短い間)、確定したあと
  if (done_) {
    font_.Draw(kDoneMessage, {centerX, y}, kInfoFontSize, WithAlphaScale(kGoldColor, alpha), BitmapFont::Align::Center);
  } else if (hasMessage_) {
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - messageTime_).count();
    if (elapsed < kMessageSeconds) {
      font_.Draw(kFullMessage, {centerX, y}, kInfoFontSize, WithAlphaScale(kWarnColor, alpha), BitmapFont::Align::Center);
    }
  }
}
