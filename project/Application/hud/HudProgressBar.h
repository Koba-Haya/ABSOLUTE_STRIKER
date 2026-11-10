#pragma once
#include "../ui/UiRectRenderer.h"
#include "Vector.h"

class BitmapFont;

/// <summary>
/// 横長のバー。見出しの文字、パーセント(固定幅の数字)、バーを描く。
/// ステージの進行と、ボスの体力のように、「0 から 1 の値を横長のバーで見せる」表示に共通で使う。
/// 分けた理由: スコアや目的の表示とは別の責務で、使い回せるため。
/// 値、見出し、色は Draw の引数で受け取り、状態は持たない(何を表すかは、呼び出し側が決める)。
/// </summary>
class HudProgressBar {
public:
  HudProgressBar() = default;
  ~HudProgressBar() = default;

  /// <summary>白い画像を読む。font は文字を描くために使う(所有しない。このバーより長く生きていること)</summary>
  void Initialize(BitmapFont *font);

  /// <summary>バー本体の位置と大きさ(画面ピクセル座標。position は左上)。文字はバーの上に出す</summary>
  void SetBounds(const Vector2 &position, const Vector2 &size);

  /// <summary>バーを描く</summary>
  /// <param name="ratio">バーの割合(0 から 1。範囲外は丸める)</param>
  /// <param name="alpha">全体の透明度(0 から 1)。出現の動きの度合いを渡す</param>
  /// <param name="label">バーの上の左に出す見出し(UTF-8)</param>
  /// <param name="fillColor">中身(割合の長さの部分)の色</param>
  void Draw(float ratio, float alpha, const char *label, const Vector4 &fillColor);

private:
  BitmapFont *font_ = nullptr;
  UiRectRenderer rects_;
  Vector2 position_{0.0f, 0.0f};
  Vector2 size_{0.0f, 0.0f};
};
