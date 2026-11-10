#pragma once
#include "UiMenuInput.h"
#include "UiRectRenderer.h"
#include "Vector.h"
#include <chrono>
#include <string>
#include <vector>

class BitmapFont;
class Input;

/// <summary>
/// 項目のリスト(選択、決定、選択中の点滅、ロック表示)を担当するクラス。
/// 項目は「文字 + ロックかどうか」だけを持つ。外枠は UiPanel が描き、このクラスはパネルの本体領域の中に描く。
/// 入力の読み取りは UiMenuInput に任せる(このクラスは、命令を受けて、カーソルを動かすだけ)。
/// ステージ選択、クリア、ゲームオーバーの結果メニューで同じ部品を使うため、シーンは項目を渡して、
/// 返ってきた Event(決定されたか)を見るだけにする。
///
/// 点滅は Update を使わず、Draw のたびに時刻から計算する(UiPanel と同じ理由)。
/// 選択を動かした直後は、いちばん明るい状態から始める(動かしたことが見て分かるように)。
/// </summary>
class UiMenuList {
public:
  /// 1 つの項目
  struct Item {
    std::string label;   // 画面に出す文字(UTF-8)
    bool locked = false; // true なら選べるが決定できない(ロック表示になる)
  };

  /// Update / Apply が返す、このフレームの出来事
  enum class Event {
    None,          // 何も起きなかった
    Moved,         // 選択が動いた
    Decided,       // 選択中の項目が決定された(GetCursor で、どの項目かを読む)
    DecidedLocked, // ロック中の項目を決定しようとした(決定にはならない。短く赤く光る)
  };

  UiMenuList();
  ~UiMenuList();

  /// <summary>font は文字を描くために使う(所有しない。メニューより長く生きていること)。白い画像も読む</summary>
  void Initialize(BitmapFont *font);

  /// <summary>項目を入れ替える。選択は先頭に戻る</summary>
  void SetItems(std::vector<Item> items);

  /// <summary>選択を index にする(範囲外なら何もしない)。戻ってきたときに前の選択を復元するのに使う</summary>
  void SetCursor(int index);

  /// <summary>いま選択中の項目の位置(項目が無いときは 0)</summary>
  int GetCursor() const { return cursor_; }

  /// <summary>項目の数</summary>
  int GetItemCount() const { return static_cast<int>(items_.size()); }

  /// <summary>全項目を並べたときの高さ(px)。項目の数に合わせて UiPanel の大きさを決めるのに使う(項目が無ければ 0)</summary>
  float GetContentHeight() const;

  /// <summary>選択中の項目(項目が無いときは nullptr)</summary>
  const Item *GetSelectedItem() const;

  /// <summary>1 フレームの入力を読んで、選択を動かす / 決定する。入力を受けたくない間(遷移中など)は呼ばない</summary>
  Event Update(const Input *input);

  /// <summary>入力を介さずに命令を直接与える(展示モードの自動決定など)</summary>
  Event Apply(UiMenuInput::Command command);

  /// <summary>
  /// 項目を描く。
  /// </summary>
  /// <param name="origin">リストの左上(UiPanel::GetBodyOrigin)</param>
  /// <param name="width">リストの幅(UiPanel::GetBodySize の x)</param>
  /// <param name="alpha">全体の薄さ(UiPanel::GetContentAlpha を渡すと、パネルと一緒に現れる)</param>
  void Draw(const Vector2 &origin, float width, float alpha);

private:
  /// 選択を step だけ動かす(端を越えたら反対側へ回り込む)
  void MoveCursor_(int step);
  /// 点滅の明るさの割合(0 から 1)。選択を動かした直後が 1
  float GetBlinkRatio_() const;
  /// ロック中の項目を決定しようとしたあとの、赤い光の強さ(0 から 1。時間とともに 0 へ)
  float GetRejectFlash_() const;

  BitmapFont *font_ = nullptr;
  UiRectRenderer rects_;
  std::vector<Item> items_;
  int cursor_ = 0;
  std::chrono::steady_clock::time_point lastMoveTime_;   // 選択を最後に動かした時刻(点滅の基準)
  std::chrono::steady_clock::time_point lastRejectTime_; // ロック項目を最後に拒んだ時刻
  bool hasRejected_ = false;
};
