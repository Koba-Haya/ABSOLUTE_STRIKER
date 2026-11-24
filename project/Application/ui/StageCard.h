#pragma once
#include <chrono>
#include "UiPanel.h"
#include "Vector.h"

class BitmapFont;
class StageProgress;
struct StageDefinition;

/// <summary>
/// ステージ選択の「カード」。選んだステージの番号(STAGE 01)、名前、世界観の説明文を、UiPanel の中に描く。
/// ロック中のステージは、名前と説明文を伏せて「???」とロックの表示にする(中身を先に見せないため)。
/// 分けた理由: 「ステージ 1 本の紹介を描く」は、ステージ選択の他の処理(入力、一覧、背景)と別の責務で、
/// 将来ほかの画面(出撃前の確認など)でも使えるようにするため。シーンは SetStage で表示するステージを渡し、Draw を呼ぶだけ。
/// </summary>
class StageCard {
public:
  StageCard();
  ~StageCard();

  /// <summary>font は文字を描くために使う。progress は、ステージが解放されているかを調べるために使う(読むだけ)。
  /// どちらも所有しない。カードより長く生きていること</summary>
  void Initialize(BitmapFont *font, const StageProgress &progress);

  /// <summary>カードの位置と大きさ(画面ピクセル座標。position は左上)</summary>
  void SetBounds(const Vector2 &position, const Vector2 &size);

  /// <summary>出現の動きを始める(待ち秒の意味は UiPanel::Open と同じ)</summary>
  void Open(float delaySeconds = 0.0f);

  /// <summary>表示するステージを決める。stage は、呼び出し側が持ち続けること(StageCatalog のもの)。nullptr なら中身を描かない</summary>
  /// <param name="stage">表示するステージ</param>
  /// <param name="number">ステージの番号(1 から。一覧の並びで決まる)</param>
  void SetStage(const StageDefinition *stage, int number);

  /// <summary>
  /// 鍵が外れる演出を始める(新しく解放されたステージを、初めて見せるとき)。SetStage のあとに呼ぶ(SetStage は、演出を止める)。
  /// 中身がすっかり現れてから、鍵が震えて外れて落ち、名前と説明文が「???」から現れる(約 1.2 秒)。
  /// それまでの間は、解放済みのステージでも、ロック中の表示のままにする
  /// </summary>
  void PlayUnlockAnimation();

  /// <summary>
  /// 鍵が外れる演出を、始めてよいか。遷移(水滴の覆い)の最中は false にして、晴れてから true にする(遷移の最中に、始まってしまわないため)。
  /// false の間は、演出を始めるのを待つ(その間も、ロック中の表示のまま)
  /// </summary>
  void SetUnlockAnimationAllowed(bool allowed) { unlockAllowed_ = allowed; }

  /// <summary>鍵が外れる演出が、終わったか(PlayUnlockAnimation を呼んでいないときは false)</summary>
  bool IsUnlockAnimationFinished() const;

  /// <summary>カード(外枠と中身)を描く</summary>
  void Draw();

  /// <summary>中身の薄さ(UiPanel::GetContentAlpha と同じ。カードの外の文字を一緒に出すときに使う)</summary>
  float GetContentAlpha() const { return panel_.GetContentAlpha(); }

private:
  const StageProgress *progress_ = nullptr; // ステージの進行(所有しない。Initialize で受け取る)

  /// 中身(番号、名前、説明文、ロック表示)を描く
  void DrawContent_(float alpha);
  /// 鍵の演出の進行を進める(待ち -> 再生中 -> 終わり)。contentAlpha は、中身の薄さ(すっかり現れたら、演出を始める)
  void UpdateUnlockState_(float contentAlpha);
  /// 鍵の演出が始まってからの経過(秒)
  float GetUnlockElapsed_() const;

  /// 鍵の演出の状態
  enum class UnlockState {
    None,    // 演出なし
    Waiting, // 始めるのを待っている(中身が現れるまで)
    Playing, // 再生中
    Finished // 終わった
  };

  BitmapFont *font_ = nullptr;
  UiPanel panel_;
  UiRectRenderer rects_; // 名前の下の区切り線用
  const StageDefinition *stage_ = nullptr;
  int number_ = 0;
  UnlockState unlockState_ = UnlockState::None;
  bool unlockAllowed_ = false; // 演出を始めてよいか(シーンが、遷移の最中は false にする)
  std::chrono::steady_clock::time_point unlockStart_; // 鍵の演出が始まった時刻(UiPanel と同じく、時刻から計算するので、Update を持たない)
};
