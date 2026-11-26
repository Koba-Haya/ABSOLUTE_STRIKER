#pragma once
#include "IResultFlowState.h"
#include <optional>

// クリア画面の段階: メニュー(もう一度 / ステージ選択へ / タイトルへ)。最後の段階。
// 通常: 選ばれた項目の画面へ切り替える。展示: 操作を受け付けず、一定時間(ExhibitionMode::kResultHoldSec)たったらステージ選択へ
// (ステージ選択で、解放された次のステージの鍵が外れる演出を見せる)。
// 次の段階は無い(画面が入れ替わるまで続く)。
class ResultMenuState : public IResultFlowState {
public:
  // すぐ開く(継承を確定した直後)
  static constexpr float kOpenNow = 0.0f;

  // openDelaySeconds: メニューが現れ始めるまでの待ち(秒)。std::nullopt なら、メニューを開かない
  // (今回の結果が無いまま展示で来たとき。以前と同じく、メニューを出さずに、時間でステージ選択へ進む)
  explicit ResultMenuState(std::optional<float> openDelaySeconds) : openDelaySeconds_(openDelaySeconds) {}

  void Enter(IResultFlowActions &actions) override;
  std::unique_ptr<IResultFlowState> Update(IResultFlowActions &actions, float deltaTime) override;

private:
  std::optional<float> openDelaySeconds_;
  float exhibitionTimer_ = 0.0f; // 展示: この段階に入ってからの経過(秒。画面の切り替えの最中は数えない)
};
