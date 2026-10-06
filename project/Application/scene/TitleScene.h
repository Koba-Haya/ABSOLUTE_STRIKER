#pragma once
#include "BaseScene.h"
#include "ModelInstance.h"
#include "title/TitleDirector.h"
#include <memory>

struct ID3D12GraphicsCommandList;

class Renderer;
class Sprite;
class UnifiedPipeline;

class TitleScene final : public BaseScene {
public:
  TitleScene() = default;
  ~TitleScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

protected:
  // タイトル画面はレール演出を持たないためタイムライン機能は不要
  bool UsesTimeline() const override { return false; }

private:
  // PRESS START の点滅アルファ(0〜1)を返す。time は秒
  static float CalcBlinkAlpha(float time);

  // ロゴのフェードインアルファ(0〜1)。time は秒
  static float CalcLogoAlpha(float time);

  // スタート演出の白フラッシュのアルファ(0〜1)。launchU: スタート演出の進行(0〜1)
  static float CalcFlashAlpha(float launchU);

  static void ApplyTitleLights(Renderer *renderer);

  // スタート入力を受け付ける(演出中は無視する)
  void RequestStart();

  bool startRequested_ = false;   // スタート演出に入ったか
  bool leaving_ = false;          // ゲームへの遷移を要求済みか
  float launchTime_ = 0.0f;       // スタート演出の経過秒
  float flashAlpha_ = 0.0f;
  float time_ = 0.0f;
  TitleDirector director_;

  // タイトル専用の見た目(本物の PlayerComponent は使わない)
  ModelInstance playerModel_;
  // ロゴと PRESS START は板ポリの3Dモデル。ImGui に依存しないので Release でも表示される
  ModelInstance logoModel_;
  ModelInstance startModel_;
  ModelInstance flashModel_; // 白フラッシュ用の板
};
