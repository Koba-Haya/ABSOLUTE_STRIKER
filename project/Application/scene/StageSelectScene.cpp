#include "StageSelectScene.h"
#include "ExhibitionMode.h"
#include "SceneIds.h"
#include "../ScreenSize.h"
#include "../stage/StageCatalog.h"
#include "../stage/StageLaunchParams.h"
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 背景の時間を進める 1 フレームの長さ
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;

// --- 背景の種類ごとの空の見え方 ---
// 雲海: 注視点の高さ(正で空を見上げ、太陽が画面に入る)。クリア画面と同じ空にそろえる
constexpr float kCloudSeaTargetHeight = 0.30f;

/// 背景の種類から、空の注視点の高さを決める。背景を増やすときは、ここに足す(StageBackdrop の全種類を扱うこと)
float GetBackdropTargetHeight(StageBackdrop backdrop) {
  switch (backdrop) {
  case StageBackdrop::CloudSea:
    return kCloudSeaTargetHeight;
  }
  return kCloudSeaTargetHeight; // 到達しない(列挙の追加忘れがあっても、雲海の空で表示を続ける)
}

// --- 画面の配置(px) ---
// 文字の資源(U1 で作ったアトラスと文字表。AssetPath の論理パス)
constexpr const char *kFontAtlasPath = "fonts/atlas/ui_font.png";
constexpr const char *kFontMetricsPath = "fonts/atlas/ui_font.json";
// カードと一覧のパネルの幅(同じ幅で、画面の中央にそろえる)
constexpr float kPanelWidth = 560.0f;
// カードの高さ
constexpr float kCardHeight = 330.0f;
// カードと一覧の間隔
constexpr float kPanelGap = 20.0f;
// 画面の下端に残す、操作の案内のための高さ(カードと一覧は、この分を除いた範囲の中央に置く)
constexpr float kHintReserveHeight = 56.0f;
// 操作の案内の、画面の下端からの距離と文字の大きさ
constexpr float kHintBottomMargin = 36.0f;
constexpr float kHintFontSize = 20.0f;
// 出現の動きの待ち(秒)。遷移の覆いが晴れる間に始まってしまうのを避ける。一覧は、カードから少し遅らせる
constexpr float kCardOpenDelaySec = 0.35f;
constexpr float kListOpenDelaySec = 0.50f;
// 中央にそろえるときの係数
constexpr float kHalf = 0.5f;

// --- 文字列(画面に出す日本語) ---
constexpr const char *kListTitle = "ステージ選択";
constexpr const char *kHintText = "↑ ↓  で選ぶ      決定  で出撃";

// --- 色(RGBA) ---
const Vector4 kHintTextColor{0.85f, 0.92f, 1.00f, 1.00f};

#ifdef USE_IMGUI
// 開発用の一覧の表示(ImGui)
constexpr float kListWindowWidth = 320.0f;
constexpr float kListWindowMargin = 16.0f;
constexpr float kTitleFontScale = 2.0f;
constexpr float kEntryHeight = 40.0f;
#endif
} // namespace

void StageSelectScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  backdrop_.Initialize(kCloudSeaTargetHeight); // 最初の空。選択が決まったら、SyncSelection_ がそのステージの背景に切り替える

  // UI: 文字、カード、一覧のパネル、メニュー
  font_.Initialize(kFontAtlasPath, kFontMetricsPath);
  card_.Initialize(&font_);
  listPanel_.Initialize(&font_);
  listPanel_.SetTitle(kListTitle);
  menu_.Initialize(&font_);

  // メニューの項目は、ステージの一覧(StageCatalog)から作る。ロック中のステージは locked になる
  const auto &stages = StageCatalog::GetInstance().GetStages();
  std::vector<UiMenuList::Item> items;
  items.reserve(stages.size());
  for (const StageDefinition &stage : stages) {
    items.push_back({stage.displayName, !stage.unlocked});
  }
  menu_.SetItems(std::move(items));

  // クリアやゲームオーバーから戻ったときは、遊んでいたステージにカーソルを合わせる(荷物が無い、または一覧に無ければ先頭のまま)
  if (const auto *params = TryGetLaunchPayload<StageLaunchParams>()) {
    for (int i = 0; i < static_cast<int>(stages.size()); ++i) {
      if (stages[i].id == params->stageId) {
        menu_.SetCursor(i);
        break;
      }
    }
  }

  // 配置を決め、パネルを開き始める(遷移の覆いが晴れるころに開く)
  LayoutPanels_();
  card_.Open(kCardOpenDelaySec);
  listPanel_.Open(kListOpenDelaySec);
  SyncSelection_();
}

void StageSelectScene::Finalize() {}

void StageSelectScene::LayoutPanels_() {
  // 一覧のパネルの高さは、項目の数で決まる(カードは固定)。2 つを縦に並べ、操作の案内の分を除いた範囲の中央に置く
  const float listHeight = UiPanel::GetHeightForBody(menu_.GetContentHeight());
  const float stackHeight = kCardHeight + kPanelGap + listHeight;
  const float left = (ScreenSize::kWidth - kPanelWidth) * kHalf;
  const float top = (ScreenSize::kHeight - kHintReserveHeight - stackHeight) * kHalf;

  card_.SetBounds({left, top}, {kPanelWidth, kCardHeight});
  listPanel_.SetBounds({left, top + kCardHeight + kPanelGap}, {kPanelWidth, listHeight});
}

void StageSelectScene::SyncSelection_() {
  if (menu_.GetCursor() == shownCursor_) return;
  shownCursor_ = menu_.GetCursor();

  const StageDefinition &stage = StageCatalog::GetInstance().GetStages()[shownCursor_];
  card_.SetStage(&stage, shownCursor_ + 1); // ステージの番号は、一覧の並びで決まる(1 から)
  ApplyBackdrop_(stage);
}

void StageSelectScene::ApplyBackdrop_(const StageDefinition &stage) {
  backdrop_.SetTargetHeight(GetBackdropTargetHeight(stage.backdrop));
}

void StageSelectScene::Update() {
  backdrop_.Update(kFrameDeltaSeconds);
  if (ExhibitionMode::kEnabled) {
    UpdateExhibition_();
  } else {
    UpdateInput_();
  }
  SyncSelection_();

#ifdef USE_IMGUI
  // 開発用の一覧(補助。本番の画面は Draw のカードと一覧)。パネルと重ならないよう、画面の左上に出す
  const auto &stages = StageCatalog::GetInstance().GetStages();
  ImGui::SetNextWindowPos(ImVec2(kListWindowMargin, kListWindowMargin), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(kListWindowWidth, 0.0f));
  ImGui::Begin("Stage Select", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

  ImGui::SetWindowFontScale(kTitleFontScale);
  ImGui::Text("STAGE SELECT");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Separator();

  for (int i = 0; i < static_cast<int>(stages.size()); ++i) {
    const StageDefinition &stage = stages[i];
    const std::string label = stage.displayName + (stage.unlocked ? "" : "  [LOCKED]") + "##" + stage.id;
    if (!stage.unlocked) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Selectable(label.c_str(), i == menu_.GetCursor(), 0, ImVec2(0.0f, kEntryHeight))) {
      menu_.SetCursor(i);
      DecideCursorStage_();
    }
    if (!stage.unlocked) {
      ImGui::EndDisabled();
    }
  }

  ImGui::End();
#endif
}

void StageSelectScene::Draw() {
  backdrop_.Begin();
  // 空の上に、カード、一覧、操作の案内の順に重ねる(2D は EndRenderScene の前に描くと、ポストエフェクトの対象になる。結果画面の帯と同じ)
  card_.Draw();

  listPanel_.Draw();
  const float listAlpha = listPanel_.GetContentAlpha();
  if (listAlpha > 0.0f) {
    // 一覧(選択、点滅、ロック表示は UiMenuList が担当する)
    menu_.Draw(listPanel_.GetBodyOrigin(), listPanel_.GetBodySize().x, listAlpha);
  }

  // 操作の案内: 画面の下端の中央。一覧と一緒に現れる
  if (listAlpha > 0.0f) {
    const float hintY = ScreenSize::kHeight - kHintBottomMargin - font_.GetLineHeight(kHintFontSize);
    font_.Draw(kHintText, {ScreenSize::kWidth * kHalf, hintY}, kHintFontSize,
               {kHintTextColor.x, kHintTextColor.y, kHintTextColor.z, kHintTextColor.w * listAlpha},
               BitmapFont::Align::Center);
  }
  backdrop_.End();
}

void StageSelectScene::UpdateExhibition_() {
  // 遷移の覆いが晴れてから数える(覆われている間に、決定してしまわないため)
  if (IsSceneTransitioning()) return;
  exhibitionHoldTimer_ += kFrameDeltaSeconds;
  if (exhibitionHoldTimer_ >= ExhibitionMode::kStageSelectHoldSec) {
    DecideCursorStage_();
  }
}

void StageSelectScene::UpdateInput_() {
  // 遷移の最中は受け付けない(前のシーンで押していたボタンで、すぐに決定してしまうのを防ぐ)
  if (!GetInput() || IsSceneTransitioning()) return;

  // 入力の読み取りと、カーソルの移動、ロックの判定は UiMenuList に任せる。ここでは「決定されたか」だけを見る
  if (menu_.Update(GetInput()) == UiMenuList::Event::Decided) {
    DecideCursorStage_();
  }
}

void StageSelectScene::DecideCursorStage_() {
  const StageDefinition &stage = StageCatalog::GetInstance().GetStages()[menu_.GetCursor()];
  if (!stage.unlocked) return;
  RequestSceneChange(SceneId::Game, StageLaunchParams{stage.id});
}
