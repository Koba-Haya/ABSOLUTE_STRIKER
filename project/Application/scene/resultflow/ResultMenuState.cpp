#include "ResultMenuState.h"
#include "ExhibitionMode.h"
#include "IResultFlowActions.h"

void ResultMenuState::Enter(IResultFlowActions &actions) {
  if (openDelaySeconds_) {
    actions.OpenMenu(*openDelaySeconds_);
  }
}

std::unique_ptr<IResultFlowState> ResultMenuState::Update(IResultFlowActions &actions, float deltaTime) {
  if (actions.IsExhibition()) {
    // 展示: 操作を受け付けず、一定時間たったら、ステージ選択へ進む
    if (!actions.IsTransitioning()) {
      exhibitionTimer_ += deltaTime;
      if (exhibitionTimer_ >= ExhibitionMode::kResultHoldSec) {
        actions.RequestSceneFor(ResultMenu::Choice::StageSelect);
      }
    }
    return nullptr;
  }
  // 通常: 決定は、メニューの 1 つのボタンだけ。選ばれた画面へ切り替える
  const ResultMenu::Choice choice = actions.UpdateMenu();
  if (choice != ResultMenu::Choice::None) {
    actions.RequestSceneFor(choice);
  }
  return nullptr;
}
