#pragma once
#include <memory>

class GameCamera;

// クリア / ゲームオーバー画面の背景(空)。
// これらの画面の文字は ImGui だけで、3D の描画が何もなかった。画面遷移の覆い(最終パス)は、シーンの描画の
// 終わり(EndRenderScene)で動くので、何も描かないシーンでは遷移が見えない。そこで、空を 3D のシーンとして描く。
class ResultBackdrop {
public:
  // targetHeight: 注視点の高さ(カメラの前方 1 ユニットに対する y)。正で空を見上げ(太陽が見える)、
  //               負で地平線の下を見下ろす(暗く霞んだ色になる)
  void Initialize(float targetHeight);
  void Update(float deltaTime) { time_ += deltaTime; }
  // 空を描き始める(この呼び出しのあとに、見出しなどのスプライトを Draw してよい)
  void Begin();
  // シーンの描画を終える(遷移の最終パスもここで動く)。Begin のあとで呼ぶ
  void End();

private:
  std::shared_ptr<GameCamera> camera_;
  float time_ = 0.0f;
};
