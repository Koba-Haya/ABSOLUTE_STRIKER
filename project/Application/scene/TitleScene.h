#pragma once
#include "BaseScene.h"
#include "ModelInstance.h"
#include "title/TitleDirector.h"
#include <array>
#include <cstdint>
#include <memory>

struct ID3D12GraphicsCommandList;

class Sprite;
class UnifiedPipeline;

// 翼のプルームを出す先端の数(片側)
constexpr int kContrailTipCount = 3;

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

  // スタート入力を受け付ける(演出中は無視する)
  void RequestStart();

  // 翼の先端から、白い蒸気のようなプルーム(軌跡)を出す。前フレームの先端位置との間を埋めて切れ目を作らない
  void EmitContrails(const Matrix4x4 &playerWorld);

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
  std::array<ModelInstance, kTitleStreakCount> streakModels_; // 気流の筋

  // 翼のプルーム: 発生点(左右 × 先端 kContrailTipCount か所)。先端はモデルの頂点から求めた右翼の位置(モデル空間)
  static constexpr int kContrailPointCount = kContrailTipCount * 2;
  std::array<Vector3, kContrailTipCount> wingTips_{};
  Vector3 modelCenter_{0.0f, 0.0f, 0.0f};
  // 前フレームの先端のワールド位置(軌跡の隙間を埋めるため)
  std::array<Vector3, kContrailPointCount> contrailPrevTip_{};
  bool contrailHasPrev_ = false;
  uint32_t contrailSeed_ = 1;
};
