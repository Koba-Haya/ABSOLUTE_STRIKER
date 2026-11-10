#include "HudStatusView.h"
#include "../ScreenSize.h"
#include "HudStyle.h"
#include <cstdio>

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

// --- 配置(px) ---
// 画面の占有を抑えるため、全体を小さくしている(パネルは UiPanel の倍率で、寸法の比率を保ったまま縮める)
constexpr float kPanelScale = 0.6f;           // パネルの寸法の倍率(枠、タイトルバー、余白、飾り)
constexpr float kScreenMargin = HudStyle::kScreenMargin; // 画面の端からパネルまでの間隔
constexpr float kObjectivePanelWidth = 230.0f;
constexpr float kScorePanelWidth = 200.0f;
constexpr float kGaugeBarWidth = 300.0f;
constexpr float kGaugeBarHeight = 10.0f;
constexpr float kGaugeBarTop = 40.0f;         // バーの上端の y(バーの上に見出しの文字と下地が収まる高さ)
constexpr float kHalf = 0.5f;
// 自機の HP のバー(左下。上のバーと同じ大きさ)。バーの左端は、パネルの左端にそろえる
constexpr float kHpBarLeft = kScreenMargin;
constexpr float kHpBarBottomMargin = kScreenMargin; // バーの下端と画面の下端の間隔

// --- 文字の大きさ(px) ---
constexpr float kObjectiveFontSize = 20.0f;
constexpr float kScoreFontSize = 28.0f;
constexpr float kKillsFontSize = 16.0f;
constexpr float kScoreLineGap = 2.0f; // スコアと撃破数の間隔

// --- 文字列(画面に出す日本語) ---
constexpr const char *kObjectiveTitle = "目的";
constexpr const char *kScoreTitle = "スコア";
constexpr const char *kObjectiveAdvance = "敵を倒しながら進め";
constexpr const char *kObjectiveDefeatBoss = "ボスを倒せ";
constexpr const char *kGaugeLabelProgress = "進行";
constexpr const char *kGaugeLabelBossHp = "ボス体力";
constexpr const char *kHpLabel = "HP";
constexpr const char *kScoreFormat = "%0*d";  // 0 埋めの固定桁(数字は固定幅なので、桁がぶれない)
constexpr int kScoreDigits = 8;
constexpr const char *kKillsFormat = "撃破  %03d";
constexpr size_t kTextBufferSize = 64;

// --- 色(RGBA) ---
const Vector4 kObjectiveColor{1.00f, 1.00f, 1.00f, 1.00f};
const Vector4 kScoreColor{1.00f, 1.00f, 1.00f, 1.00f};
const Vector4 kKillsColor{0.62f, 0.86f, 1.00f, 1.00f};
const Vector4 kGaugeColorProgress{0.62f, 0.86f, 1.00f, 0.95f}; // 進行: 明るい青
const Vector4 kGaugeColorBossHp{1.00f, 0.35f, 0.30f, 0.95f};   // ボスの体力: 赤(進行と見分ける)
const Vector4 kHpColorNormal{0.35f, 0.90f, 0.45f, 0.95f};      // 自機の HP: 緑
const Vector4 kHpColorLow{1.00f, 0.70f, 0.20f, 0.95f};         // 自機の HP が少ないとき: 橙
constexpr float kHpLowRatio = 0.3f;                            // この割合以下を「少ない」とする(最大 HP がいくつでも割合で判断する)

/// 色のアルファに倍率を掛けた色を返す
Vector4 WithAlphaScale(const Vector4 &c, float scale) { return {c.x, c.y, c.z, c.w * scale}; }
} // namespace

const char *HudStatusView::GetObjectiveText_(HudObjective objective) {
  switch (objective) {
  case HudObjective::Advance:
    return kObjectiveAdvance;
  case HudObjective::DefeatBoss:
    return kObjectiveDefeatBoss;
  }
  return kObjectiveAdvance; // 到達しない(列挙の追加忘れがあっても、文字は出し続ける)
}

void HudStatusView::Initialize() {
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);

  // 目的: 左上。本体は 1 行
  objectivePanel_.Initialize(&font_);
  objectivePanel_.SetScale(kPanelScale);
  objectivePanel_.SetTitle(kObjectiveTitle);
  const float objectiveHeight = UiPanel::GetHeightForBody(font_.GetLineHeight(kObjectiveFontSize), kPanelScale);
  objectivePanel_.SetBounds({kScreenMargin, kScreenMargin}, {kObjectivePanelWidth, objectiveHeight});

  // スコア: 右上。本体は 2 行(スコア、撃破数)
  scorePanel_.Initialize(&font_);
  scorePanel_.SetScale(kPanelScale);
  scorePanel_.SetTitle(kScoreTitle);
  const float scoreBodyHeight = font_.GetLineHeight(kScoreFontSize) + kScoreLineGap + font_.GetLineHeight(kKillsFontSize);
  const float scoreHeight = UiPanel::GetHeightForBody(scoreBodyHeight, kPanelScale);
  scorePanel_.SetBounds({ScreenSize::kWidth - kScreenMargin - kScorePanelWidth, kScreenMargin},
                        {kScorePanelWidth, scoreHeight});

  // バー(進行 / ボスの体力): 上の中央
  gaugeBar_.Initialize(&font_);
  gaugeBar_.SetBounds({(ScreenSize::kWidth - kGaugeBarWidth) * kHalf, kGaugeBarTop}, {kGaugeBarWidth, kGaugeBarHeight});

  // 自機の HP のバー: 左下
  hpBar_.Initialize(&font_);

  // ロックオンの表示(位置は、描くときに画面の右下から決まる)
  lockSlots_.Initialize(&font_);
  lockMarkers_.Initialize();
  hpBar_.SetBounds({kHpBarLeft, ScreenSize::kHeight - kHpBarBottomMargin - kGaugeBarHeight}, {kGaugeBarWidth, kGaugeBarHeight});
}

void HudStatusView::Draw() {
  // 最初に描くとき(HUD が初めて出るとき)に、出現の動きを始める
  if (!opened_) {
    opened_ = true;
    objectivePanel_.Open();
    scorePanel_.Open();
  }

  DrawObjective_();
  DrawScore_();
  DrawGauge_();
  DrawHp_();
  DrawLockOn_();
}

void HudStatusView::DrawObjective_() {
  objectivePanel_.Draw();
  const float alpha = objectivePanel_.GetContentAlpha();
  if (alpha <= 0.0f) return;
  font_.Draw(GetObjectiveText_(status_.objective), objectivePanel_.GetBodyOrigin(), kObjectiveFontSize,
             WithAlphaScale(kObjectiveColor, alpha));
}

void HudStatusView::DrawGauge_() {
  const bool isBossHp = status_.gauge == HudGauge::BossHp;
  gaugeBar_.Draw(status_.gaugeRatio, scorePanel_.GetContentAlpha(), isBossHp ? kGaugeLabelBossHp : kGaugeLabelProgress,
                 isBossHp ? kGaugeColorBossHp : kGaugeColorProgress);
}

void HudStatusView::DrawHp_() {
  const Vector4 &color = status_.hpRatio <= kHpLowRatio ? kHpColorLow : kHpColorNormal;
  hpBar_.Draw(status_.hpRatio, scorePanel_.GetContentAlpha(), kHpLabel, color);
}

void HudStatusView::DrawLockOn_() {
  const bool isFull = status_.lockMax > 0 && status_.lockedCount >= status_.lockMax;
  lockSlots_.Draw(status_.lockedCount, status_.lockMax, scorePanel_.GetContentAlpha());
  lockMarkers_.Draw(status_.lockingMode, status_.cursorPos, status_.lockRadius, status_.lockedPositions, isFull);
}

void HudStatusView::DrawScore_() {
  scorePanel_.Draw();
  const float alpha = scorePanel_.GetContentAlpha();
  if (alpha <= 0.0f) return;

  const Vector2 origin = scorePanel_.GetBodyOrigin();
  char text[kTextBufferSize];

  // 1 行目: スコア(0 埋めの固定桁)
  std::snprintf(text, sizeof(text), kScoreFormat, kScoreDigits, status_.score);
  font_.Draw(text, origin, kScoreFontSize, WithAlphaScale(kScoreColor, alpha));

  // 2 行目: 撃破数
  std::snprintf(text, sizeof(text), kKillsFormat, status_.kills);
  font_.Draw(text, {origin.x, origin.y + font_.GetLineHeight(kScoreFontSize) + kScoreLineGap}, kKillsFontSize,
             WithAlphaScale(kKillsColor, alpha));
}
