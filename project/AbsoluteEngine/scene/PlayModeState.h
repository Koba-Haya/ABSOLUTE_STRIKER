#pragma once
#include "IPlayState.h"

namespace AbsoluteEngine {

// 再生状態(エディットか、プレイか)を持ち、IPlayState として見せる小さなクラス。
//
// 分けた理由: BaseScene 自身に IPlayState を実装させると、シーンの責務が増える(BaseScene の分割を進めてきた方向に逆行する)。
// 状態を持つだけの部品として切り出し、シーンはこれを持って、部品へは読み取り専用の窓口(IPlayState)だけを渡す。
class PlayModeState final : public IPlayState {
public:
  PlayMode GetPlayMode() const override { return mode_; }
  void SetPlayMode(PlayMode mode) { mode_ = mode; }

private:
  PlayMode mode_ = PlayMode::Edit; // 起動時はエディット
};

} // namespace AbsoluteEngine
