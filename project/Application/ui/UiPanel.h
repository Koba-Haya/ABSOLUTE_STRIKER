#pragma once
#include "UiRectRenderer.h"
#include "Vector.h"
#include <chrono>
#include <string>

class BitmapFont;

/// <summary>
/// UI のパネル(背景、枠、四隅の飾り、タイトルバー)と、その出現の動きを担当するクラス。
/// 中身(メニューの項目、説明文など)は描かない。中身を置く場所(本体領域)を GetBodyOrigin / GetBodySize で返し、
/// 中身の描画は呼び出し側が行う(責務を「外枠」だけにして、選択画面、ルート看板、ポーズで共通に使うため)。
/// 見た目は、落ち着いた戦術端末風(暗い青の半透明の背景、細い明るい枠、濃い青のタイトルバー)。
///
/// 出現の動き: Open を呼んでからの経過時間で、背景が中心から上下に開き、そのあとタイトルと中身が薄く現れる。
/// Update は持たない。Draw のたびに「Open からの経過時間」を std::chrono::steady_clock で測って計算する。
/// 理由: 状態を毎フレーム進める処理が要らず、フレームレートが変わっても動きの長さが変わらず、
/// 呼び出し側が「Update を呼び忘れる」ミスも起きないため。
/// </summary>
class UiPanel {
public:
  UiPanel();
  ~UiPanel();

  /// <summary>白い画像を読む。font は文字(タイトル)を描くために使う(所有しない。パネルより長く生きていること)</summary>
  void Initialize(BitmapFont *font);

  /// <summary>パネルの位置と大きさを決める(画面ピクセル座標。position は左上。出現の動きが終わったあとの大きさ)</summary>
  void SetBounds(const Vector2 &position, const Vector2 &size);

  /// <summary>
  /// パネルの寸法(枠の太さ、タイトルバーの高さ、タイトルの文字、余白、四隅の飾り)の倍率。既定は 1。
  /// ゲーム中の HUD のように、画面を広く占めたくない小さなパネルで、見た目の比率を保ったまま縮めるために使う。
  /// Initialize のあと、SetBounds や GetHeightForBody を使う前に決めること
  /// </summary>
  void SetScale(float scale) { scale_ = scale; }

  /// <summary>タイトルバーに出す文字(UTF-8)</summary>
  void SetTitle(const std::string &title);

  /// <summary>出現の動きを始める。delaySeconds だけ待ってから動き出す(遷移の覆いが晴れるのを待つときなどに使う)</summary>
  void Open(float delaySeconds = 0.0f);

  /// <summary>パネルを描く。Open を呼ぶ前は何も描かない</summary>
  void Draw();

  /// <summary>
  /// 中身を出してよい度合い(0 で非表示、1 で完全に表示)。出現の後半で 0 から 1 へ上がる。
  /// 呼び出し側は、中身の色のアルファにこの値を掛ける(パネルと一緒に中身が現れるようにするため)
  /// </summary>
  float GetContentAlpha() const;

  /// <summary>
  /// パネル全体の透明度の倍率(1 で通常、0 で消える)。出現の動きとは別に掛かる。
  /// 演出で、パネルを後から薄く消したいとき(リトライの演出で見出しと一緒に消す、など)に使う。
  /// GetContentAlpha にも掛かるので、中身(呼び出し側が描く文字)も一緒に薄くなる
  /// </summary>
  void SetOpacityScale(float scale) { opacityScale_ = scale; }

  /// <summary>出現の動きが終わったか(Open を呼ぶ前は false)</summary>
  bool IsOpened() const;

  /// <summary>
  /// 本体領域の高さ(中身が必要とする高さ)から、パネル全体の高さを求める(タイトルバーと余白を足す)。
  /// 中身の量に合わせてパネルの大きさを決めるときに使う(余白の寸法を、呼び出し側が知らなくて済む)
  /// </summary>
  /// <param name="scale">SetScale に渡す倍率と同じ値(既定 1)</param>
  static float GetHeightForBody(float bodyHeight, float scale = 1.0f);

  /// <summary>本体領域(タイトルバーの下)の左上と大きさ。内側の余白は引いてある</summary>
  Vector2 GetBodyOrigin() const;
  Vector2 GetBodySize() const;

private:
  /// 出現の進み具合(0 から 1)。Open の前、待ち時間の間は 0
  float GetProgress_() const;
  /// 背景と枠を描く(出現の途中は、中心から上下に開いた高さで描く)
  void DrawFrame_(float openness);
  /// 四隅の飾り、タイトルバー、タイトル文字を描く(出現の終盤から)
  void DrawDecoration_(float contentAlpha);

  BitmapFont *font_ = nullptr;
  UiRectRenderer rects_;
  Vector2 position_{0.0f, 0.0f};
  Vector2 size_{0.0f, 0.0f};
  std::string title_;
  float scale_ = 1.0f;                              // 寸法の倍率
  float opacityScale_ = 1.0f;                       // 全体の透明度の倍率
  bool opened_ = false;                             // Open を呼んだか
  std::chrono::steady_clock::time_point startTime_; // 動き出す時刻(Open の時刻 + 待ち)
};
