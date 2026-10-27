#pragma once
#include <memory>
#include <string>

class Sprite;

// クリア / ゲームオーバー画面の見出しや案内(「GAME OVER」「RETRY / TITLE」などの画像)。
// 文字は ImGui だけだと、Release(ImGui なし)では何も出ないので、スプライトで出す。
// シーンの 3D の描画の最中(ResultBackdrop::Begin と End の間)に Draw() を呼ぶこと。
// 責務は「画像 1 枚を、遅れて現れる形で、画面の決まった位置に出す」だけ。
// 現れ方は 2 通り:
//   Update(deltaTime)          : シーンの開始からの時間で現れる(クリア画面)
//   SetRevealProgress(progress): 画面遷移の覆いが晴れる進み具合に合わせて現れる(ゲームオーバー画面)
class ResultBanner {
public:
  // 画像の大きさと位置、現れる区間の指定
  struct Layout {
    float width = 720.0f;          // 表示の幅(ピクセル。画面は 1280 x 720)
    float height = 180.0f;         // 表示の高さ(ピクセル)
    float centerYRatio = 0.42f;    // 中心の高さ(画面の高さに対する割合。0 が上端、1 が下端)
    float revealStart = 0.25f;     // SetRevealProgress の、現れ始める進み具合(0〜1)
    float revealEnd = 0.65f;       // SetRevealProgress の、完全に現れる進み具合(0〜1)
  };

  ResultBanner();
  ~ResultBanner(); // Sprite を前方宣言で持つので、実装は .cpp へ

  // texturePath: 画像(背景は透明)
  void Initialize(const std::string &texturePath, const Layout &layout = Layout{});
  // 経過時間を進める(時間で現れる演出用)
  void Update(float deltaTime);
  // 遷移の晴れる進み具合(0〜1)から、現れ具合を決める(遷移で現れる演出用)。毎フレーム呼ぶ
  void SetRevealProgress(float progress);
  void Draw();

private:
  // 0〜1 の値を、なめらかな 0〜1 の現れ具合へ直す
  static float Smooth_(float t);

  std::unique_ptr<Sprite> sprite_;
  Layout layout_{};
  float time_ = 0.0f;
  float appearance_ = 0.0f; // 現れ具合(0 = 透明、1 = 完全に見える)
};
