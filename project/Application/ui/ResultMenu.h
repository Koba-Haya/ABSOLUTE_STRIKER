#pragma once
#include "BitmapFont.h"
#include "UiMenuList.h"
#include "UiPanel.h"

class Input;

/// <summary>
/// クリア画面とゲームオーバー画面の、共通の結果メニュー(「もう一度」「ステージ選択へ」「タイトルへ」)。
/// 画面の下の中央に、UiPanel の枠つきで項目を出す。項目の選択と決定は UiMenuList、入力の読み取りは UiMenuInput に任せ、
/// このクラスは「結果画面で何を選べるか」と、決定された項目を、シーンが扱いやすい Choice に直す責務だけを持つ。
/// 分けた理由: クリアとゲームオーバーで、項目、見た目、操作を同じにするため。シーンは Update の返り値(Choice)を見て、
/// 遷移を要求するだけにする(キーの割り当てが、結果画面ごとに違ってしまうのを防ぐ)。
/// 見た目のために、背景に敷くパネルを持つ(クリア画面の空は明るく、文字だけでは読みにくいため)。
/// </summary>
class ResultMenu {
public:
  /// 決定された項目
  enum class Choice {
    None,        // 何も決定されなかった
    Retry,       // もう一度(同じステージ)
    StageSelect, // ステージ選択へ
    Title,       // タイトルへ
  };

  ResultMenu() = default;
  ~ResultMenu() = default;

  /// <summary>文字の資源とパネル、項目を用意する。ファイルが無いときは、探した場所を表示して終了する</summary>
  void Initialize();

  /// <summary>現れる動きを始める(待ち秒の意味は UiPanel::Open と同じ)。呼ぶまでは、何も描かない</summary>
  void Open(float delaySeconds = 0.0f);

  /// <summary>現れる動きを始めたか</summary>
  bool IsOpen() const { return opened_; }

  /// <summary>現れる動きが終わったか(入力を受け付けてよい目安)</summary>
  bool IsOpened() const { return opened_ && panel_.IsOpened(); }

  /// <summary>全体の透明度の倍率(1 で通常、0 で消える)。リトライの演出で、見出しと一緒に消すのに使う</summary>
  void SetOpacityScale(float scale) { panel_.SetOpacityScale(scale); }

  /// <summary>
  /// 1 フレームの入力を読んで、決定された項目を返す。
  /// </summary>
  /// <param name="input">入力</param>
  /// <param name="acceptInput">false の間は入力を読まない(遷移中、演出中、現れる前など)</param>
  /// <returns>決定された項目(何も決定されなければ None)</returns>
  Choice Update(const Input *input, bool acceptInput);

  /// <summary>メニューを描く。シーンの 3D の描画の最中(雲の合成のあと)に呼ぶ</summary>
  void Draw();

private:
  BitmapFont font_;  // 文字(日本語可)
  UiPanel panel_;    // 枠(出現の動き、透明度)
  UiMenuList menu_;  // 項目
  bool opened_ = false;
};
