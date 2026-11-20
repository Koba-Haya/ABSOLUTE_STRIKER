#pragma once
#include "UiRectRenderer.h"
#include "Vector.h"

/// <summary>
/// 「段階」を、数字ではなく、枠の並びで見せる部品(ゲーム中のロックオン数 HudLockOnSlots と同じ見た目)。
/// 枠の数が上限、色で埋まった分が「いまの値」、その続きの別の色が「これから増える分」。
/// 値は小数でもよく、1 つの枠の途中までを埋める(例: 1.5 なら、1 つ目は全部、2 つ目は半分)。
/// 分けた理由: 「個数を枠の並びで見せる」は、継承の選択(強化の段階、取った数、枠の使用数)で何度も使う描き方で、
/// 画面ごとに書き写すと見た目がずれるため。値の意味は持たず、渡された数を描くだけ。
///
/// 使い方: Initialize(1 回)-> Draw(毎フレーム。シーンの 3D の描画の最中に呼ぶ)
/// </summary>
class UiPipBar {
public:
  /// 見た目の指定(枠の寸法と色)
  struct Style {
    Vector2 pipSize{26.0f, 12.0f};              // 1 つの枠の大きさ(px)
    float gap = 4.0f;                           // 枠と枠の間隔(px)
    Vector4 baseColor{0.62f, 0.86f, 1.0f, 0.95f}; // 「いまの値」の色
    Vector4 gainColor{1.0f, 0.90f, 0.45f, 1.0f};  // 「これから増える分」の色(点滅の強さは gainBlink)
    float alpha = 1.0f;                         // 全体の透明度(0 から 1。パネルの現れ具合を渡す)
    float gainBlink = 1.0f;                     // 増える分の濃さ(0 から 1。呼び出す側が、時間で変える)
  };

  /// <summary>白い画像を読む</summary>
  void Initialize();

  /// <summary>
  /// 枠の並びを描く。
  /// </summary>
  /// <param name="topLeft">並びの左上(画面ピクセル)</param>
  /// <param name="count">枠の数(上限。0 以下なら何も描かない)</param>
  /// <param name="baseValue">いまの値(枠の単位。小数可)。0 から count に収める</param>
  /// <param name="gainValue">増える分(枠の単位。小数可)。baseValue の続きから、count を超えない範囲で描く</param>
  /// <param name="style">寸法と色</param>
  void Draw(const Vector2 &topLeft, int count, float baseValue, float gainValue, const Style &style);

  /// <summary>count 個の枠を並べたときの全体の幅(px)</summary>
  static float GetWidth(int count, const Style &style);

private:
  UiRectRenderer rects_;
};
