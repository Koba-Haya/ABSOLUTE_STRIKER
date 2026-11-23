#pragma once
#include "BitmapFont.h"
#include "../ui/UiRectRenderer.h"

/// <summary>
/// 展示モードの切り替え操作(TAB キー)を知らせる、画面の右上の小さな案内。常に、ゆっくり点滅して出す。
/// 通常のときは「TAB 展示モード」、展示中は「TAB 展示モードを終了」と出す。
/// 位置は、画面の上端のすぐ下(HUD は、画面の端から 20px の内側に置くので、重ならない)。
/// 分けた理由: 「どの画面でも出す案内」は、各シーンの見た目とは別の責務。シーンは、描画の最中に Draw を呼ぶだけにする。
/// 全シーンで共有するので、唯一のインスタンスにする(文字の資源を 1 回だけ読むため)。
/// 使い方: アプリの初期化で Initialize(1 回)-> 各シーンが、2D の描画の最中に Draw を呼ぶ(雲の合成のあと、EndRenderScene の前)
/// </summary>
class ExhibitionHint {
public:
  static ExhibitionHint &GetInstance();

  /// <summary>文字の資源と、白い画像を読む(1 回)</summary>
  void Initialize();

  /// <summary>案内を描く(点滅は、時刻から計算する)。Initialize の前は、何もしない</summary>
  /// <param name="exhibitionEnabled">展示モードに入っているか(文言を切り替える)</param>
  void Draw(bool exhibitionEnabled);

private:
  ExhibitionHint() = default;

  BitmapFont font_;
  UiRectRenderer rects_;
  bool ready_ = false;
};
