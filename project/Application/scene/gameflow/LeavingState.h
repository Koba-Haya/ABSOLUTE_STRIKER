#pragma once
#include "IGameFlowState.h"

// ゲームの進行の状態: 終了の要求済み(クリアかゲームオーバーへの切り替えを要求して、シーンが入れ替わるのを待つ間)。
//
// 以前の GamePhase::Clear と GamePhase::GameOver を 1 つにまとめたもの。2 つは、どこでも区別されておらず、
// 振る舞いも同じ(何も受け付けない。二重に要求しない)だったため。進む先は無い。
// 処理が無いので、ヘッダだけで完結させている(.cpp を作らない)。
class LeavingState : public IGameFlowState {
public:
  std::unique_ptr<IGameFlowState> Update(IGameFlowActions &actions) override {
    (void)actions;
    return nullptr;
  }
};
