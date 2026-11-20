#pragma once
#include <string>

#include "BitmapFont.h"

/// <summary>
/// 「決定ボタンで進む」などの案内の文字を、ゆっくり点滅させて出す部品。
/// 分けた理由: 「入力を待っていることを知らせる」は、結果の集計(ResultTally)や継承の選択(InheritancePicker)とは
/// 別の責務で、ほかの画面でも使い回せるため。入力は読まない(押されたかの判断は、呼び出す側のシーンが行う)。
///
/// 使い方: Initialize(1 回) -> SetPosition -> Show(案内の文字) -> Update / Draw(毎フレーム) -> 不要になったら Hide
/// </summary>
class UiContinuePrompt {
public:
  /// <summary>文字の資源を用意する。ファイルが無いときは、探した場所を表示して終了する</summary>
  void Initialize();

  /// <summary>案内の位置を決める。centerX は文字列の中心の x、centerY は文字列の中心の y(画面ピクセル)</summary>
  void SetPosition(float centerX, float centerY);

  /// <summary>案内を出し始める(すでに出ているときは、何もしない。点滅の位相を保つため)。text は UTF-8 の文字列</summary>
  void Show(const std::string &text);

  /// <summary>案内を、すぐに消す</summary>
  void Hide();

  /// <summary>案内が出ているか</summary>
  bool IsVisible() const { return visible_; }

  /// <summary>時間を進める(1 フレームの長さ。秒)</summary>
  void Update(float deltaTime);

  /// <summary>描く。シーンの 3D の描画の最中(雲の合成のあと)に呼ぶ</summary>
  void Draw();

private:
  BitmapFont font_;
  std::string text_;
  float centerX_ = 0.0f;
  float centerY_ = 0.0f;
  float time_ = 0.0f; // 出し始めてからの経過(秒)
  bool visible_ = false;
};
