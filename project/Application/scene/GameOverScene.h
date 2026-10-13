#pragma once
#include "BaseScene.h"
#include "ResultBackdrop.h"
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
  float exhibitionHoldTimer_ = 0.0f; // 展示: 覆いが晴れてからの経過(秒)
  ResultBanner banner_;     // 見出し(スプライト)
  ResultBackdrop backdrop_; // 背景の空(遷移の覆いが描画を通るようにもする)
};
