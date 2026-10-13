#pragma once
#include "graphics/TransitionStyle.h"

// 1 回の画面遷移の設定(見た目と時間)。
struct TransitionSpec {
  TransitionStyle style;     // 見た目(種類・色など)
  float outSeconds = 0.6f;   // 画面を覆い切るまでの時間(0 なら、遷移の開始と同時に完全に覆う)
  float holdSeconds = 0.15f; // 覆い切ったまま保つ時間。シーンの入れ替え(重い初期化)は、この区間の頭で行う
  float inSeconds = 0.6f;    // 覆いが晴れるまでの時間
  // 雲を抜ける遷移: 入れ替え後のシーンの世界での、雲の層の下面・上面。
  // 入れ替え前の層は style.slabBottom / slabTop。シーンごとに世界の座標が違うため、前後で別に指定する
  float slabBottomAfter = 40.0f;
  float slabTopAfter = 100.0f;
};

// 画面遷移の時間の進行だけを管理する(描画もシーンの入れ替えも知らない)。
//   Out(覆う) → シーンを入れ替えてよい瞬間(ConsumeSwitchRequest が true) → Hold(覆ったまま保つ) → In(晴れる)
// 時間は、呼び出し側が渡す deltaTime で進める。固定の 1 フレーム分を渡せば、シーンの重い初期化で
// 実時間が飛んでも、遷移の進行は飛ばない。
class SceneTransition {
public:
  // 遷移を始める。進行中なら、新しい設定で最初から始め直す
  void Begin(const TransitionSpec &spec);
  // 遷移を中断して、覆いをなくす
  void Cancel();
  // 時間を進める。遷移中でなければ何もしない
  void Update(float deltaTime);

  // 遷移の進行中(覆っている、保っている、晴れている途中)か
  bool IsActive() const { return phase_ != Phase::Idle; }
  // 晴れている途中か(新しいシーンが表示され始めている)
  bool IsRevealing() const { return phase_ == Phase::In; }
  // 画面が完全に覆われた瞬間に、1 度だけ true を返す。true のフレームに、シーンを入れ替える
  bool ConsumeSwitchRequest();

  // 覆い具合(0 = 透明、1 = 完全に覆う)
  float GetCover() const;
  // 晴れる途中の進み具合(0 = 晴れ始める前、1 = 完全に晴れた。時間に比例する。遷移中でなければ 1)
  float GetRevealProgress() const;
  // 晴れる区間の長さ(秒)
  float GetInSeconds() const { return spec_.inSeconds; }
  // 遷移を始めてからの経過(秒)。覆い具合と違い、晴れている間も増え続ける(雲の流れや水滴の流れに使う)
  float GetElapsedSeconds() const { return totalElapsed_; }
  // 遷移全体の長さ(覆う + 保つ + 晴れる。秒)
  float GetTotalSeconds() const { return spec_.outSeconds + spec_.holdSeconds + spec_.inSeconds; }
  // 今の区間に使う見た目。雲の層は、入れ替え前(覆っている途中)と、入れ替え後(保つ・晴れる)で切り替える
  TransitionStyle GetStyle() const;

private:
  enum class Phase { Idle, Out, Hold, In };

  // 現在の区間の長さと、そこまでの経過から 0〜1 の進み具合を返す(長さ 0 なら 1)
  float Progress_(float duration) const;
  void EnterPhase_(Phase next);

private:
  TransitionSpec spec_{};
  Phase phase_ = Phase::Idle;
  float elapsed_ = 0.0f; // 現在の区間に入ってからの経過(秒)
  float totalElapsed_ = 0.0f; // 遷移を始めてからの経過(秒)
  bool switchRequested_ = false;
};
