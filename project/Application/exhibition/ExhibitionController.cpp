#include "ExhibitionController.h"

#include "Input.h"
#include "Logger.h"
#include "SceneIds.h"
#include "SceneManager.h"
#include "../inheritance/InheritanceState.h"
#include "../scene/ExhibitionMode.h"
#include "../stage/StageCatalog.h"
#include "../stage/StageLaunchParams.h"
#include "../stage/StageProgress.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 展示モードの切り替えキー(F12 は、デバッガの一時停止に使われることがあるので、TAB にした)
constexpr int kToggleKey = DIK_TAB;
} // namespace

void ExhibitionController::Update(const Input &input, SceneManager &sceneManager) {
#ifdef USE_IMGUI
  // 開発用の窓で、文字を入力している間は、切り替えない
  if (ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) return;
#endif
  if (!input.TriggerKey(kToggleKey)) return;
  // 画面遷移の最中は受け付けない(遷移の要求が、無視されて、状態だけ変わってしまうのを防ぐ)
  if (sceneManager.IsTransitioning()) return;

  if (!ExhibitionMode::IsEnabled()) {
    // 入る: 実際のデータを退避して、仮の状態にし、ゲームの最初から始める
    StageProgress::GetInstance().BeginVolatile();
    InheritanceState::GetInstance().BeginVolatile();
    ExhibitionMode::SetEnabled(true);
    const auto &stages = StageCatalog::GetInstance().GetStages();
    Logger::GetInstance().Log("[Exhibition] enter");
    sceneManager.RequestChange(SceneId::Game, StageLaunchParams{stages.empty() ? std::string{} : stages.front().id});
  } else {
    // 出る: 退避したデータへ戻して、タイトルへ戻る
    StageProgress::GetInstance().EndVolatile();
    InheritanceState::GetInstance().EndVolatile();
    ExhibitionMode::SetEnabled(false);
    Logger::GetInstance().Log("[Exhibition] leave");
    sceneManager.RequestChange(SceneId::Title);
  }
}

void ExhibitionController::ResetLoopState() {
  StageProgress::GetInstance().Reset();
  InheritanceState::GetInstance().Reset();
}
