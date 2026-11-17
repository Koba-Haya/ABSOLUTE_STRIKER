#include "ResultTally.h"
#include "../ScreenSize.h"
#include "../result/RatingCatalog.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

// --- 配置(px) ---
constexpr float kRowsPanelLeft = 90.0f;      // 左のパネルの左端
constexpr float kRowsPanelWidth = 700.0f;    // 左のパネルの幅
constexpr float kPanelGap = 20.0f;           // 2 つのパネルの間隔
constexpr float kRightMargin = 90.0f;        // 右のパネルの右端と、画面の右端の間隔
constexpr float kMaxRowHeight = 46.0f;       // 1 行の高さの上限(縦の余裕が大きいとき、間延びさせない)
constexpr float kMaxLabelFontSize = 24.0f;   // 項目名の文字の大きさの上限(行の高さが足りないときは、小さくする)
constexpr float kMinLabelFontSize = 14.0f;   // 項目名の文字の大きさの下限
constexpr float kDetailFontRatio = 0.72f;    // 数字の説明の文字の大きさ(項目名に対する割合)
constexpr float kDetailX = 230.0f;           // 数字の説明の左端(本体領域の左から)
constexpr float kBarHeight = 3.0f;           // 割合の棒の高さ
constexpr float kBarGap = 1.0f;              // 文字の行と、割合の棒の間隔
constexpr float kRowBottomGap = 2.0f;        // 割合の棒と、行の下端の間隔
constexpr float kTotalFontSize = 26.0f;      // 合計の点数の文字の大きさ
constexpr float kRankFontSize = 110.0f;      // ランクの文字の大きさ(押し終わったあと。アトラスの文字を、大きく引き伸ばすと、かくかくになる)
constexpr float kRankStampStartScale = 1.6f; // ランクが押される前の、文字の拡大の倍率
constexpr float kHalf = 0.5f;

// --- 時間(秒。Begin からの経過) ---
constexpr float kPanelOpenSeconds = 0.8f;    // パネルが開き終わる(集計の項目が現れ始める)までの時間(待ちのあと)
constexpr float kRowInterval = 0.50f;        // 項目が現れる間隔
constexpr float kRowFadeSeconds = 0.25f;     // 項目が現れる(薄い -> 濃い)時間
constexpr float kRowCountSeconds = 0.60f;    // 項目の数字が数え上がる時間
constexpr float kTotalGapSeconds = 0.35f;    // 最後の項目が数え終わってから、合計が始まるまでの間
constexpr float kTotalCountSeconds = 0.90f;  // 合計が数え上がる時間
constexpr float kStampGapSeconds = 0.30f;    // 合計が数え終わってから、ランクが押されるまでの間
constexpr float kStampSeconds = 0.35f;       // ランクが押される(大きい -> 元の大きさ)時間
constexpr float kFlashSeconds = 0.30f;       // ランクが押された瞬間の、光る時間
constexpr float kFinishGapSeconds = 0.50f;   // ランクが押されてから、集計が終わるまでの間
constexpr float kRankPanelDelaySeconds = 0.15f; // 右のパネルが、左より遅れて開く時間
constexpr float kFlashAlpha = 0.5f;          // ランクが押された瞬間の、光の最大の濃さ

// --- 色(RGBA) ---
const Vector4 kLabelColor{1.00f, 1.00f, 1.00f, 1.00f};        // 項目名
const Vector4 kDetailColor{0.78f, 0.86f, 0.94f, 1.00f};       // 数字の説明
const Vector4 kPointsColor{1.00f, 0.92f, 0.62f, 1.00f};       // 点数(金色)
const Vector4 kBarBackColor{0.20f, 0.28f, 0.38f, 0.60f};      // 割合の棒の背景
const Vector4 kBarFillColor{0.62f, 0.86f, 1.00f, 0.95f};      // 割合の棒(水色)
const Vector4 kTotalColor{1.00f, 1.00f, 1.00f, 1.00f};        // 合計
const Vector4 kFlashColor{1.00f, 0.95f, 0.75f, 1.00f};        // ランクが押された瞬間の光
// ランクごとの色
const Vector4 kRankColorS{1.00f, 0.84f, 0.30f, 1.00f}; // 金
const Vector4 kRankColorA{1.00f, 0.55f, 0.25f, 1.00f}; // 橙
const Vector4 kRankColorB{0.45f, 0.78f, 1.00f, 1.00f}; // 水色
const Vector4 kRankColorC{0.70f, 0.74f, 0.80f, 1.00f}; // 灰

// --- 文字列(画面に出す日本語) ---
constexpr const char *kRowsPanelTitle = "結果";
constexpr const char *kRankPanelTitle = "評価";
constexpr const char *kLabelScore = "スコア";
constexpr const char *kLabelKill = "撃破率";
constexpr const char *kLabelDamage = "被ダメージ";
constexpr const char *kLabelFormation = "編隊の全滅";
constexpr const char *kLabelMultiLock = "マルチロック";
constexpr const char *kLabelBossNoDamage = "ボス戦ノーダメージ";
constexpr const char *kTotalLabel = "合計";
constexpr size_t kTextBufferSize = 96;

float Saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

// 始めは速く、終わりへ向けてゆるやかに止まる(1 - (1 - t)^3)
float EaseOutCubic(float t) {
  const float inv = 1.0f - Saturate(t);
  return 1.0f - inv * inv * inv;
}

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

// 整数を、3 桁ごとに区切った文字にする(5100 -> "5,100")
std::string FormatWithCommas(int value) {
  std::string digits = std::to_string(value < 0 ? -value : value);
  for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
    digits.insert(static_cast<size_t>(i), ",");
  }
  return value < 0 ? "-" + digits : digits;
}
} // namespace

void ResultTally::Initialize() {
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);
  rowsPanel_.Initialize(&font_);
  rowsPanel_.SetTitle(kRowsPanelTitle);
  rankPanel_.Initialize(&font_);
  rankPanel_.SetTitle(kRankPanelTitle);
  rects_.Initialize();
}

void ResultTally::SetVerticalRange(float top, float bottom) {
  top_ = top;
  bottom_ = bottom;
}

void ResultTally::BuildRows_() {
  const RatingSettings &settings = RatingCatalog::GetInstance().GetSettings();
  const RunStats &stats = result_.stats;
  char text[kTextBufferSize];
  rows_.clear();

  // スコア
  std::snprintf(text, sizeof(text), "%s / 目標 %s", FormatWithCommas(result_.score).c_str(), FormatWithCommas(result_.scoreTarget).c_str());
  rows_.push_back({kLabelScore, text, &result_.scoreEntry, settings.weightScore});

  // 撃破率(自滅した敵は、母数から外している)
  const int eligible = stats.enemiesSpawned - stats.enemiesSelfDestructed;
  std::snprintf(text, sizeof(text), "%d / %d 体 (%d%%)", stats.enemiesKilled, eligible,
                static_cast<int>(std::lround(result_.killEntry.ratio * 100.0f)));
  rows_.push_back({kLabelKill, text, &result_.killEntry, settings.weightKill});

  // 被ダメージ
  if (stats.damageTaken == 0) {
    std::snprintf(text, sizeof(text), "ノーダメージ");
  } else {
    std::snprintf(text, sizeof(text), "%d ダメージ (%d 回被弾)", stats.damageTaken, stats.hitCount);
  }
  rows_.push_back({kLabelDamage, text, &result_.damageEntry, settings.weightDamage});

  // 編隊の全滅
  if (stats.formationsSpawned == 0) {
    std::snprintf(text, sizeof(text), "編隊なし");
  } else {
    std::snprintf(text, sizeof(text), "%d / %d 編隊", stats.formationsCleared, stats.formationsSpawned);
  }
  rows_.push_back({kLabelFormation, text, &result_.formationEntry, settings.weightFormation});

  // マルチロック
  std::snprintf(text, sizeof(text), "最大 %d 同時ロック / 一斉撃破 %d", stats.maxLockCount, stats.maxKillsInWindow);
  rows_.push_back({kLabelMultiLock, text, &result_.multiLockEntry, settings.weightMultiLock});

  // ボス戦ノーダメージ
  if (stats.bossHitCount == 0) {
    std::snprintf(text, sizeof(text), "達成");
  } else {
    std::snprintf(text, sizeof(text), "ボス戦で %d 回被弾", stats.bossHitCount);
  }
  rows_.push_back({kLabelBossNoDamage, text, &result_.bossNoDamageEntry, settings.weightBossNoDamage});

  totalMaxPoints_ = settings.weightScore + settings.weightKill + settings.weightDamage + settings.weightFormation +
                    settings.weightMultiLock + settings.weightBossNoDamage;
}

float ResultTally::RowStartTime_(int index) const {
  return kPanelOpenSeconds + static_cast<float>(index) * kRowInterval;
}

void ResultTally::Begin(const RunResult &result, float delaySeconds) {
  result_ = result;
  BuildRows_();
  active_ = true;
  time_ = 0.0f;

  // パネルの配置: 左右に並べて、画面の中央にそろえる。高さは、渡された縦の範囲いっぱい
  const float height = (std::max)(bottom_ - top_, 0.0f);
  const float rankLeft = kRowsPanelLeft + kRowsPanelWidth + kPanelGap;
  const float rankWidth = ScreenSize::kWidth - kRightMargin - rankLeft;
  rowsPanel_.SetBounds({kRowsPanelLeft, top_}, {kRowsPanelWidth, height});
  rankPanel_.SetBounds({rankLeft, top_}, {rankWidth, height});
  rowsPanel_.Open(delaySeconds);
  rankPanel_.Open(delaySeconds + kRankPanelDelaySeconds);

  // 時刻の計画(この時刻は、パネルが開き始める時刻(delaySeconds)を 0 とする経過)
  const float lastRowEnd = RowStartTime_(static_cast<int>(rows_.size()) - 1) + kRowCountSeconds;
  totalStart_ = lastRowEnd + kTotalGapSeconds;
  stampStart_ = totalStart_ + kTotalCountSeconds + kStampGapSeconds;
  finishTime_ = stampStart_ + kStampSeconds + kFinishGapSeconds;
  // 待ち(delaySeconds)の間は、時間を進めない(Update が、待ちを引いてから進める)
  waitSeconds_ = delaySeconds;
}

void ResultTally::Skip() {
  if (!active_) return;
  time_ = (std::max)(time_, finishTime_);
  waitSeconds_ = 0.0f; // 待ちも飛ばす(パネルの開く動きは、UiPanel が、自分の時刻で続ける)
}

void ResultTally::Update(float deltaTime) {
  if (!active_) return;
  if (waitSeconds_ > 0.0f) {
    waitSeconds_ -= deltaTime; // パネルが開き始めるのを待つ間
    return;
  }
  time_ += deltaTime;
}

void ResultTally::Draw() {
  if (!active_) return;
  rowsPanel_.Draw();
  rankPanel_.Draw();
  const float rowsAlpha = rowsPanel_.GetContentAlpha();
  if (rowsAlpha > 0.0f) DrawRows_(rowsAlpha);
  const float rankAlpha = rankPanel_.GetContentAlpha();
  if (rankAlpha > 0.0f) DrawRank_(rankAlpha);
}

void ResultTally::DrawRows_(float alpha) {
  const Vector2 origin = rowsPanel_.GetBodyOrigin();
  const Vector2 body = rowsPanel_.GetBodySize();
  const float rowHeight = (std::min)(body.y / static_cast<float>(rows_.size()), kMaxRowHeight);
  // 文字の大きさ: 1 行(文字の行 + 割合の棒 + 余白)が、行の高さに収まる大きさ。収まらないほど狭いときは、文字を小さくする
  // (行の高さが足りないと、割合の棒が文字に重なって、文字に横線が入って見える)
  const float lineHeightPerSize = font_.GetLineHeight(1.0f);
  const float textHeightBudget = rowHeight - kBarHeight - kBarGap - kRowBottomGap;
  const float labelFontSize = std::clamp(textHeightBudget / lineHeightPerSize, kMinLabelFontSize, kMaxLabelFontSize);
  const float detailFontSize = labelFontSize * kDetailFontRatio;
  const float pointsFontSize = labelFontSize;

  for (size_t i = 0; i < rows_.size(); ++i) {
    const Row &row = rows_[i];
    const float t = time_ - RowStartTime_(static_cast<int>(i));
    if (t <= 0.0f) continue; // まだ現れない
    const float rowAlpha = alpha * Saturate(t / kRowFadeSeconds);
    const float count = EaseOutCubic(t / kRowCountSeconds); // 数え上がりの進み具合(0 〜 1)

    const float y = origin.y + static_cast<float>(i) * rowHeight;
    // 項目名(左)、数字の説明(中)、点数(右)。文字の高さが違うので、行の中央にそろえて置く
    const float labelLineHeight = font_.GetLineHeight(labelFontSize);
    const float detailY = y + (labelLineHeight - font_.GetLineHeight(detailFontSize)) * kHalf;
    font_.Draw(row.label, {origin.x, y}, labelFontSize, WithAlphaScale(kLabelColor, rowAlpha));
    font_.Draw(row.detail, {origin.x + kDetailX, detailY}, detailFontSize, WithAlphaScale(kDetailColor, rowAlpha));
    char points[kTextBufferSize];
    std::snprintf(points, sizeof(points), "+%.1f / %.0f", row.entry->points * count, row.maxPoints);
    font_.Draw(points, {origin.x + body.x, y}, pointsFontSize, WithAlphaScale(kPointsColor, rowAlpha), BitmapFont::Align::Right);

    // 割合の棒: 文字の行の、すぐ下。背景の上に、割合まで水色が伸びる(文字には重ならない)
    const float barY = y + labelLineHeight + kBarGap;
    rects_.Fill(UiRect{origin.x, barY, body.x, kBarHeight}, WithAlphaScale(kBarBackColor, rowAlpha));
    rects_.Fill(UiRect{origin.x, barY, body.x * row.entry->ratio * count, kBarHeight}, WithAlphaScale(kBarFillColor, rowAlpha));
  }
}

void ResultTally::DrawRank_(float alpha) {
  const Vector2 origin = rankPanel_.GetBodyOrigin();
  const Vector2 body = rankPanel_.GetBodySize();
  const float centerX = origin.x + body.x * kHalf;

  // 合計(上): 数え上がる
  if (time_ >= totalStart_) {
    const float count = EaseOutCubic((time_ - totalStart_) / kTotalCountSeconds);
    char total[kTextBufferSize];
    std::snprintf(total, sizeof(total), "%s %.1f / %.0f", kTotalLabel, result_.totalPoints * count, totalMaxPoints_);
    font_.Draw(total, {centerX, origin.y}, kTotalFontSize, WithAlphaScale(kTotalColor, alpha), BitmapFont::Align::Center);
  }

  // ランク(中央): 押される(大きい -> 元の大きさ。同時に、濃くなる)
  if (time_ >= stampStart_) {
    const float t = Saturate((time_ - stampStart_) / kStampSeconds);
    const float eased = EaseOutCubic(t);
    const float scale = kRankStampStartScale + (1.0f - kRankStampStartScale) * eased;
    const float fontSize = kRankFontSize * scale;
    const Vector4 color = WithAlphaScale(RankColor(result_.rank), alpha * Saturate(t * 3.0f));
    // 行ボックスの中心を、本体領域の中央(合計の下)へ合わせる
    const float centerY = origin.y + font_.GetLineHeight(kTotalFontSize) + (body.y - font_.GetLineHeight(kTotalFontSize)) * kHalf;
    const float lineHeight = font_.GetLineHeight(fontSize);
    font_.Draw(RankToString(result_.rank), {centerX, centerY - lineHeight * kHalf}, fontSize, color, BitmapFont::Align::Center);

    // 押された瞬間の光(すぐ収まる)
    const float flashT = Saturate((time_ - stampStart_ - kStampSeconds * kHalf) / kFlashSeconds);
    if (flashT > 0.0f && flashT < 1.0f) {
      rects_.Fill(UiRect{origin.x, origin.y, body.x, body.y}, WithAlphaScale(kFlashColor, kFlashAlpha * (1.0f - flashT) * alpha));
    }
  }
}
