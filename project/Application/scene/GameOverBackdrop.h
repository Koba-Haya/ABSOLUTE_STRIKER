#pragma once
#include "graphics/SeaSkySettings.h"
#include "graphics/VolumeCloudSettings.h"
#include <memory>

class GameCamera;
class Sprite;

// ゲームオーバー画面の背景。死亡演出で蝶が落ちた雲海(ゲームの背景と同じ空と雲の層)を、
// 雲の上から、地平線を見るように描く。そのまま描くと明るすぎる(落ちた直後の曇天の気分に合わない)ので、
// 雲の光を弱め、空には暗い膜(スプライト)を重ねて、暗い曇天にする。
// ResultBackdrop(クリア画面の空)と同じ使い方(Begin と End の間に、見出しなどのスプライトを描く)。
// クラスを分けた理由: ResultBackdrop は「空だけの背景」で、こちらは「雲海 + 暗くする膜」と持ち物が違うため
class GameOverBackdrop {
public:
  GameOverBackdrop();
  ~GameOverBackdrop(); // Sprite を前方宣言で持つので、実装は .cpp へ

  void Initialize();
  void Update(float deltaTime) { time_ += deltaTime; }
  // 空を描き始め、雲を依頼する
  void Begin();
  // 雲を描いてシーンへ合成する。見出しなどのスプライトは、これを呼んだあとに Draw する
  // (雲は深度を見て重なり、深度を持たないスプライトは、雲より前に描くと雲に隠れてしまうため)
  void ResolveClouds();
  // シーンの描画を終える(遷移の最終パスもここで動く)。ResolveClouds のあとで呼ぶ
  void End();

private:
  std::shared_ptr<GameCamera> camera_;
  std::unique_ptr<Sprite> dim_; // 空を暗くする膜(画面全体の黒い半透明)
  SeaSkySettings seaSky_;
  VolumeCloudSettings cloudLayer_;
  float time_ = 0.0f;
};
