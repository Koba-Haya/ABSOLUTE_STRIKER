#pragma once
#include "IOpeningState.h"
#include "StageStartPose.h"
#include <memory>

class IOpeningActions;

// 開幕の演出(イントロ -> 俯瞰へ引く -> 保つ -> 戻りながら板を突き破る)の進行を持つクラス(State パターンの、状態を持つ側)。
//
// 役目: 今の状態を 1 つ持ち、毎フレーム Update を呼び、状態が返した次の状態へ切り替える。次にどこへ進むかは、状態クラスが決める。
// ゲームの進行(GameFlow)とは別の状態機械にしている。理由: 戻る段階は、道中と同時に進む(世界が動き、敵も撃つ)。
// State パターンは「同時に 1 つだけ」の状態を表すので、同時に成り立つ 2 つの関心事(ゲームのルールと、開幕の見せ方)は、別々に持つ。
// 状態が無い(nullptr)ときは、演出をしていない(始まっていない、または終わった)。
class OpeningSequence {
public:
  // イントロから始める(今の状態があれば、置き換える)。settings: ステージ開始演出の設定(各状態へ渡す)
  void Start(const StageStartSettings &settings);
  // 演出を止める(状態を手放す)。エディタの Stop で、プレイの状態を最初へ戻すときに使う
  void Stop() { state_.reset(); }

  // 今の状態を進める。状態が次の状態を返したら切り替え、切り替えた状態を、時間 0 でもう一度進める
  // (段階の長さが 0 のときや、余りが次の段階の長さを超えたときに、同じフレームで先へ進めるため。以前の、全体の経過時間から段階を求める形と同じ結果になる)。
  // 最後の段階が終わったら、状態を手放す。演出をしていないときは、何もしない
  void Update(IOpeningActions &actions, float deltaTime);

  // 演出の最中か
  bool IsActive() const { return state_ != nullptr; }
  // 以下は、今の状態への問い合わせ(意味は IOpeningState の同名の関数を参照)。演出をしていないときは、false / ずれなし / 0 を返す
  bool IsWorldRunning() const { return state_ && state_->IsWorldRunning(); }
  bool AppliesViewOffset() const { return state_ && state_->AppliesViewOffset(); }
  StageStartPose GetPose() const { return state_ ? state_->GetPose() : StageStartPose{}; }
  float GetBoost() const { return state_ ? state_->GetBoost() : 0.0f; }

private:
  // 1 フレームに切り替える回数の上限(状態の数。段階は戻らないので、これを超えることはない。万一の無限ループを防ぐ)
  static constexpr int kMaxTransitionsPerFrame = 4;

  std::unique_ptr<IOpeningState> state_; // 今の状態(演出をしていないときは nullptr)
};
