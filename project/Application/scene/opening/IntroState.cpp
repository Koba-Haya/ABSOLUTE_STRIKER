#include "IntroState.h"
#include "IOpeningActions.h"
#include "PullBackState.h"

std::unique_ptr<IOpeningState> IntroState::Update(IOpeningActions &actions, float deltaTime) {
  if (!actions.IsTransitionRevealing()) {
    // 覆いが晴れ終わった。カメラと自機を基準の位置へ戻してから、俯瞰へ引く段階へ進む。
    // イントロは時間で進まないので、このフレームの時間は、まるごと次の段階の経過として渡す
    // (以前も、イントロが終わったフレームで、ステージ開始演出の時間を 1 フレーム分進めていた)
    actions.FinishIntro();
    return std::make_unique<PullBackState>(settings_, deltaTime);
  }
  actions.ApplyIntroPose();
  return nullptr;
}
