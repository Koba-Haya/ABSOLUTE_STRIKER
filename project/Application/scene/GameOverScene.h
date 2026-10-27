#pragma once
#include "BaseScene.h"
#include "GameOverBackdrop.h"
#include "ResultBanner.h"

class GameOverScene final : public BaseScene {
public:
  GameOverScene() = default;
  ~GameOverScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

protected:
  // ゲームオーバー画面はレール演出を持たないためタイムライン機能は不要
  bool UsesTimeline() const override { return false; }

private:
  // 展示: 操作を受け付けず、覆いが晴れてから一定時間たったらゲームへ戻る
  void UpdateExhibition_();
  // 通常: 決定(タイトル)とリトライの入力を受け付ける。遷移の覆いが晴れきるまでは受け付けない
  void UpdateInput_();
#ifdef USE_IMGUI
  // デバッグ用の小さな窓(Title / Retry のボタン)。画面の隅に出して、見出しと重ならないようにする
  void DrawDebugWindow_();
#endif

  float exhibitionHoldTimer_ = 0.0f; // 展示: 覆いが晴れてからの経過(秒)
  ResultBanner banner_;              // 見出し(スプライト)
  ResultBanner prompt_;              // 操作の案内(スプライト。Retry / Title)
  GameOverBackdrop backdrop_;        // 背景の雲海(遷移の覆いが描画を通るようにもする)
};
