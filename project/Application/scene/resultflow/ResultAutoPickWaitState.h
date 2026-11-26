#pragma once
#include "IResultFlowState.h"

// クリア画面の段階: 展示中の、継承の選択を始めるまでの待ち。操作を受け付けず、集計を一定時間見せる。
// 次へ進む条件: 一定時間(ExhibitionMode::kTallyHoldSec)たった -> 自動操作で継承の選択を始め、ResultPickState
class ResultAutoPickWaitState : public IResultFlowState {
public:
  std::unique_ptr<IResultFlowState> Update(IResultFlowActions &actions, float deltaTime) override;

private:
  float timer_ = 0.0f; // 集計が終わってからの経過(秒)
};
