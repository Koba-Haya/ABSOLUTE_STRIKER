#pragma once
#include "PlayMode.h"

namespace AbsoluteEngine {

// 「いまの再生状態(エディットか、プレイか)」を、読み取り専用で知るための窓口(インターフェース)。
//
// 作った理由: 自機やレールカメラの部品が、プレイ中かを知るために、グローバル(以前の BaseScene::GetActiveScene)から
// 「今のシーン」を取っていた。知りたいのは再生状態だけなので、その機能だけを窓口にした。
// 読み取り専用。切り替えは、状態を持つ側(PlayModeState)と、それを持つシーンだけが行う。
class IPlayState {
public:
  virtual ~IPlayState() = default;

  virtual PlayMode GetPlayMode() const = 0;
};

} // namespace AbsoluteEngine
