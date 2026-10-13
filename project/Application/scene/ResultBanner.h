#pragma once
#include <memory>
#include <string>

class Sprite;

// クリア / ゲームオーバー画面の見出し(「GAME CLEAR」などの画像)。
// 文字は ImGui だけだと、Release(ImGui なし)では何も出ないので、スプライトで出す。
// シーンの 3D の描画の最中(ResultBackdrop::Begin と End の間)に Draw() を呼ぶこと。
// 責務は「見出し 1 枚を、遅れて現れる形で、画面の決まった位置に出す」だけ
class ResultBanner {
public:
  ResultBanner();
  ~ResultBanner(); // Sprite を前方宣言で持つので、実装は .cpp へ

  // texturePath: 見出しの画像(横長で、背景は透明)
  void Initialize(const std::string &texturePath);
  // 経過時間を進める(現れる演出用)
  void Update(float deltaTime);
  void Draw();

private:
  std::unique_ptr<Sprite> sprite_;
  float time_ = 0.0f;
};
