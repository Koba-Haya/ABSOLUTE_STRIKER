#pragma once
#include "../ui/UiRectRenderer.h"
#include "Vector.h"

class BitmapFont;

/// <summary>
/// ロックオン数の表示(画面右下)。数字は使わず、枠の並びで見せる。
/// 枠の数が「ロックオンできる上限」、色で埋まった枠の数が「いま捕まえている数」。上限に達したら、色が橙に変わる。
/// 枠の数は上限に合わせて増減し、多いときは 1 つの枠の幅を自動で詰める(全体の幅は一定の範囲に収まる)。
/// 分けた理由: 「個数を枠の並びで見せる」は、バー(HudProgressBar)や文字の表示とは別の見せ方で、
/// 値(捕まえた数と上限)は Draw の引数で受け取るだけにして、ゲームの内部状態を直接読まないため。
/// </summary>
class HudLockOnSlots {
public:
  HudLockOnSlots() = default;
  ~HudLockOnSlots() = default;

  /// <summary>白い画像を読む。font は見出しの文字を描くために使う(所有しない。この部品より長く生きていること)</summary>
  void Initialize(BitmapFont *font);

  /// <summary>描く</summary>
  /// <param name="lockedCount">いま捕まえている数</param>
  /// <param name="maxCount">ロックオンできる上限(枠の数。0 以下なら何も描かない)</param>
  /// <param name="alpha">全体の透明度(0 から 1)。出現の動きの度合いを渡す</param>
  void Draw(int lockedCount, int maxCount, float alpha);

private:
  BitmapFont *font_ = nullptr;
  UiRectRenderer rects_;
};
