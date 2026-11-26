#pragma once
#include "StageStartPose.h"
#include <memory>

class IOpeningActions;

// 開幕の演出の「状態」の共通の形(State パターンの、状態の基底クラス)。
//
// 状態の一覧(遷移):
//   IntroState(覆いが晴れる間の助走)   -> PullBackState(覆いが晴れ終わった)
//   PullBackState(俯瞰へ引く)          -> HoldState(引く時間が過ぎた)
//   HoldState(俯瞰を保つ)              -> ReturnState(保つ時間が過ぎた)
//   ReturnState(戻りながら板を突き破る) -> 終了(戻る時間が過ぎた。IsFinished が true になる)
// 次の状態は、状態クラス自身が Update の戻り値で決める(GameFlow と同じ形)。
//
// 時間の持ち方: 各状態が、自分の段階に入ってからの経過を持つ。段階の長さを超えた分(余り)は、次の状態へ引き継ぐ。
// 以前(StageStartDirector)の「全体の経過時間から、毎回、段階を計算する」形と、同じタイミングで段階が切り替わる。
//
// 問い合わせ(IsWorldRunning など)は、GameScene が「今の段階で、世界を動かすか」「カメラをどれだけずらすか」を聞くためのもの。
// 既定は「世界は止まる、ずれなし、加速なし」。違う段階だけが上書きする。
class IOpeningState {
public:
  virtual ~IOpeningState() = default;

  // 時間を進めて、段階の中の見た目を反映する。
  // deltaTime: この状態の時間を進める秒数。切り替わった直後の状態には 0 が渡される(余りは、生成時に受け取っている)。
  // 戻り値: 次の状態へ進むときは、次の状態。この状態に留まるとき(終了したときも)は nullptr
  virtual std::unique_ptr<IOpeningState> Update(IOpeningActions &actions, float deltaTime) = 0;

  // 演出の最後の段階が終わったか(true になったら、OpeningSequence が状態を手放し、演出が終わる)
  virtual bool IsFinished() const { return false; }
  // 世界(レール・敵・自機)を動かす段階か。止まる段階では、GameScene は自機のアニメーションだけを進める
  virtual bool IsWorldRunning() const { return false; }
  // 描画の間だけ、カメラに俯瞰のずれを重ねる段階か。イントロは、カメラを実際に動かすので、重ねない
  virtual bool AppliesViewOffset() const { return true; }
  // 今の瞬間の、カメラのずれ(ブーストによる画角の広がりは含まない)
  virtual StageStartPose GetPose() const { return {}; }
  // ブースト量(0 〜 1)。世界の速さ、画角、速度線、ブラーに使う
  virtual float GetBoost() const { return 0.0f; }
};
