#include "ResultMenu.h"
#include "../ScreenSize.h"
#include <array>

namespace {
// 文字の資源(AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";

// --- 配置(px) ---
constexpr float kPanelWidth = 420.0f;       // パネルの幅
constexpr float kPanelBottomMargin = 24.0f; // パネルの下端と画面の下端の間隔
constexpr float kHalf = 0.5f;

// --- 項目(上から、画面に出す日本語と、決定されたときの Choice) ---
struct EntryDefinition {
  const char *label;
  ResultMenu::Choice choice;
};
constexpr std::array<EntryDefinition, 3> kEntries = {{
    {"もう一度", ResultMenu::Choice::Retry},
    {"ステージ選択へ", ResultMenu::Choice::StageSelect},
    {"タイトルへ", ResultMenu::Choice::Title},
}};
constexpr const char *kPanelTitle = "メニュー";
} // namespace

void ResultMenu::Initialize() {
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);
  menu_.Initialize(&font_);

  std::vector<UiMenuList::Item> items;
  items.reserve(kEntries.size());
  for (const EntryDefinition &entry : kEntries) {
    items.push_back({entry.label, false});
  }
  menu_.SetItems(std::move(items));

  // パネルは、画面の下の中央に置く(高さは項目の数で決まる)
  panel_.Initialize(&font_);
  panel_.SetTitle(kPanelTitle);
  const float height = UiPanel::GetHeightForBody(menu_.GetContentHeight());
  panel_.SetBounds({(ScreenSize::kWidth - kPanelWidth) * kHalf, ScreenSize::kHeight - kPanelBottomMargin - height},
                   {kPanelWidth, height});
}

void ResultMenu::Open(float delaySeconds) {
  if (opened_) return;
  opened_ = true;
  panel_.Open(delaySeconds);
}

ResultMenu::Choice ResultMenu::Update(const Input *input, bool acceptInput) {
  if (!acceptInput || !IsOpened()) return Choice::None;
  if (menu_.Update(input) != UiMenuList::Event::Decided) return Choice::None;
  return kEntries[menu_.GetCursor()].choice;
}

void ResultMenu::Draw() {
  if (!opened_) return;
  panel_.Draw();
  const float alpha = panel_.GetContentAlpha();
  if (alpha > 0.0f) {
    menu_.Draw(panel_.GetBodyOrigin(), panel_.GetBodySize().x, alpha);
  }
}
