#pragma once
#include <deque>
#include <string>

#include "BitmapFont.h"
#include "../ui/UiRectRenderer.h"

// 強化を取ったことを知らせる、短い文字(「最大HP +1」など)を、画面の上寄りに出すクラス。
// 分けた理由: 「取ったことを知らせる」は、強化の仕組み(ItemSystem)と、HUD の他の部品(HudStatusView など)とは別の責務。
// ItemSystem が、取った瞬間に Show を呼び、毎フレーム Update と Draw を呼ぶだけにする。
//
// 動き: 文字は、板(半透明の黒い帯)の上に、少し下から現れて、しばらく留まり、薄れて消える。
// 続けて取ったときは、新しいものが上に出て、古いものが下へ押し下げられる(同時に最大 kMaxToasts 個。あふれた古いものは、すぐ薄れる)。
// 使い方: Initialize(1 回)-> Show(取ったとき)-> Update / Draw(毎フレーム。Draw は、HUD と同じ 2D の描画の最中に呼ぶ)
class UpgradeToast {
public:
  // 文字の資源を読む(1 回)。ファイルが無いときは、探した場所を表示して終了する
  void Initialize();

  // 知らせを 1 つ出す(text は UTF-8)
  void Show(const std::string &text);

  // 時間を進める(1 フレームの実時間。秒)
  void Update(float deltaTime);

  // 知らせを描く
  void Draw();

private:
  struct Toast {
    std::string text;
    float age = 0.0f; // 出てからの経過(秒)
  };

  BitmapFont font_;
  UiRectRenderer rects_;
  std::deque<Toast> toasts_; // 新しいものが先頭
  bool ready_ = false;
};
