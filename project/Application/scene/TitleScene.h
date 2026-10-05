#pragma once
#include "BaseScene.h"
#include "ModelInstance.h"
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

  static void ApplyTitleLights(Renderer *renderer);

  bool startRequested_ = false;
  float time_ = 0.0f;

  // タイトル専用の見た目(本物の PlayerComponent は使わない)
  ModelInstance playerModel_;
  // ロゴと PRESS START は板ポリの3Dモデル。ImGui に依存しないので Release でも表示される
  ModelInstance logoModel_;
  ModelInstance startModel_;
};
