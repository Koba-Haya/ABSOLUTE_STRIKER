#pragma once
#include <string>

// ステージを始めるときに、前のシーンから GameScene へ渡す荷物の型。
// SceneManager が運ぶ荷物(std::any)の中身として使う。送る側は RequestSceneChange(SceneId::Game, StageLaunchParams{...})、
// 受ける側は TryGetLaunchPayload<StageLaunchParams>() で取り出す。
// 分けた理由: 荷物の型は、送る側(ステージ選択、クリア、ゲームオーバー)と受ける側(ゲーム)の両方が使う共通の約束なので、
// どちらのシーンにも属さない小さなヘッダにして、依存が一方向に保たれるようにするため。
// 型はコピーできること(std::any の条件)。
struct StageLaunchParams {
  // 始めるステージの ID(StageDefinition::id)
  std::string stageId;
};
