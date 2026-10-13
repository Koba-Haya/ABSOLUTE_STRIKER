#pragma once
#include <optional>
#include <string>

#include "SceneTransition.h"

// シーンの組み合わせごとの画面遷移の設定(データの表)。
// 遷移を足す・変えるときは、このファイルの表だけを触る(シーン側のコードは変えない)。
namespace SceneTransitionTable {

// (今のシーン, 次のシーン) に使う遷移の設定を返す。表にない組み合わせは std::nullopt(遷移なしで即座に切り替える)
std::optional<TransitionSpec> Resolve(const std::string &fromSceneId, const std::string &toSceneId);

} // namespace SceneTransitionTable
