#pragma once
#include "BaseScene.h"
#include "GameOverAsh.h"
#include "GameOverBackdrop.h"
#include "RebirthButterfly.h"
#include "RebirthDirector.h"
#include "ResultBanner.h"
#include "ui/ResultMenu.h"

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
  // 通常: メニューの入力(もう一度、ステージ選択へ、タイトルへ)を受け付ける。遷移の覆いが晴れきるまで、復活の演出中は受け付けない
  void UpdateInput_();
  // リトライ: 復活の演出を始める(1 回だけ。始まったあとは入力を受け付けない)
  void BeginRebirth_();
  // 復活の演出の指示を、灰・蝶・カメラ・雲・見出しへ反映する(背景の Update より前に呼ぶ)
  void UpdateRebirth_();
#ifdef USE_IMGUI
  // デバッグ用の小さな窓(Title / Retry のボタン)。画面の隅に出して、見出しと重ならないようにする
  void DrawDebugWindow_();
#endif

  float exhibitionHoldTimer_ = 0.0f; // 展示: 覆いが晴れてからの経過(秒)
  ResultBanner banner_;              // 見出し(スプライト)
  ResultMenu menu_;                  // 結果メニュー(もう一度、ステージ選択へ、タイトルへ。見通しが開いたら現れる)
  GameOverBackdrop backdrop_;        // 背景の雲海(遷移の覆いが描画を通るようにもする)
  RebirthDirector rebirth_;          // 復活の演出のタイムライン(リトライで始まる)
  RebirthButterfly butterfly_;       // 復活する蝶
  float worldTimeScale_ = 1.0f;      // 世界(雲、島、灰、粒)の時間の速さ。復活のバーストのヒットストップで下げる
  bool rebirthStarted_ = false;      // 復活の演出を始めた(以後、入力を受け付けない。シーンが入れ替わるまで続く)
  GameOverAsh ash_;                  // 雲の中を漂い、巣へ集まる灰と、巣に灯る熾火の核
};
