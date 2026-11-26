#pragma once
#include <memory>

class IResultFlowActions;

// クリア画面の「段階」の共通の形(State パターンの、状態の基底クラス)。
//
// 段階の一覧(遷移):
//   ResultTallyState(集計)            -> ResultMenuState(選ぶ強化が無い。選ばずに継承を確定)
//                                     -> ResultAutoPickWaitState(展示: 選ぶ強化がある)
//                                     -> ResultContinueState(通常: 選ぶ強化がある)
//   ResultContinueState(決定を待つ)     -> ResultPickState(決定が押された)
//   ResultAutoPickWaitState(展示の待ち) -> ResultPickState(一定時間たった。自動操作で選ぶ)
//   ResultPickState(継承の選択)        -> ResultMenuState(選び終わった。継承を確定)
//   ResultMenuState(メニュー)          -> なし(選ばれた画面へ切り替える。展示は、一定時間後にステージ選択へ)
// 今回の結果が無いとき(ゲームから結果が渡されなかった)は、ResultMenuState から始まる。
// 以前は bool 4 つ(menuOpened_ / inheritanceStarted_ / inheritanceCommitted_ と、展示の待ち)の組み合わせで段階を表していた。
// 次の段階は、段階のクラス自身が Update の戻り値で決める。切り替えは、Update のあとに ClearScene が行う(1 フレームに 1 回)。
class IResultFlowState {
public:
  virtual ~IResultFlowState() = default;

  // この段階に入ったときに 1 回呼ばれる
  virtual void Enter(IResultFlowActions &actions) { (void)actions; }

  // 毎フレーム呼ばれる。deltaTime: 前のフレームからの経過秒。
  // 戻り値: 次の段階へ進むときは、次の段階。留まるときは nullptr
  virtual std::unique_ptr<IResultFlowState> Update(IResultFlowActions &actions, float deltaTime) = 0;
};
