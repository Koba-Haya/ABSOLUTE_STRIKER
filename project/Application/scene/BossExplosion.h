#pragma once
#include <array>
#include <memory>

#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Enemy/BossModelSpec.h"
#include "ClearDirector.h"
#include "StageImpactEffects.h"
#include "Type/Vector.h"

class Renderer;

// ボス撃破の演出の、爆破の球(体の中心に生まれて、膨らみ、爆散する光の球)を受け持つクラス。
//
// 分けた理由: 球は、「大きさと濃さの時間変化」「描き方(球のモデルとシェーダー)」という、まとまった責務を持つ。
// 光線(BossLightBeams)や、外殻のひび割れ(BossDefeatVisual)とは別の部品にして、BossDefeatVisual は、サブフェーズと進み具合を渡すだけにする。
//
// 見た目(参考: ゼルダの伝説 BotW のガノン撃破の、白く飛んだ球):
//   核(プラズマ球): 不透明に近い球。中心は白く飛び、縁は赤橙。球面を、ノイズの斑が流れる(崩れる体の影)。斑は、Peel の間に薄れて、純白になる
//   光の輪(ハロ): 核より大きい、ふわっとした光の球。核の外側へ光がにじむ(ブルームの代わり)
//   衝撃波のリング: Burst で、時間差で外へ広がる細い輪(縁だけが濃い球。どの向きから見ても輪に見える)
//   フラッシュ: Burst の最初に、画面全体が一瞬白く光る(既存の StageImpactEffects を再利用)
// 球は不透明に近いので、中の物は、球に隠れる(粒を球の中に溜めて、爆散で解き放つ演出に使う)。
// 動き:
//   Peel  : 崩壊と同時に、体の中心から球が膨らむ(始めは速く、終わりへ向けてゆるやか)。斑が薄れて、白くなる
//   Burst : 爆散。フラッシュが光り、核は薄れ、ハロが広がって消え、リングが外へ広がる
//   それ以外 : 描かない
// 使い方: Initialize(シーンの開始時に 1 回。モデルの読み込みを、演出の最中にしないため) -> Begin(演出の開始時) -> Update / Draw / DrawFlash(毎フレーム)

namespace BossExplosionDefaults {
// 球のモデル(半径 1 の球。テクスチャは使わない。exe 基準のパス)
constexpr const char *kModelPath = "resources/app/bossBurst/sphere.obj";
constexpr const char *kFlashModelPath = "resources/app/title/flash.obj"; // フラッシュの白い板(x・y が -1 〜 1 の四角形)
// 核
constexpr float kStartRadius = 0.5f;      // Peel の始まりの半径(ワールド単位)
constexpr float kMaxRadius = 7.0f;        // Peel の終わりの半径(ボスは幅 約 9、全長 約 14。体をほぼ包む大きさ)
constexpr float kBurstGrow = 1.3f;        // Burst の間に、さらに広がる倍率(Peel の終わりの半径に対する)
constexpr float kRampShare = 0.12f;       // Peel の始めの、濃さが立ち上がる割合(0 〜 1)
constexpr float kRimPower = 2.2f;         // 縁の鋭さ(大きいほど、縁の細い輪だけが色づき、白い面が広がる)
constexpr float kPatchStart = 0.85f;      // Peel の始めの、斑の量(0 〜 1)。Peel の間に 0 へ薄れる
inline const Vector3 kRimColor{1.0f, 0.28f, 0.10f}; // 縁と斑の色(赤橙。明るい空の上でも、白く飛ばない)
// ハロ(核の外側へにじむ光)
constexpr float kHaloScale = 2.0f;        // 核の半径に対する、ハロの半径の倍率
constexpr float kHaloAlpha = 0.7f;        // ハロの最大の濃さ
constexpr float kHaloPower = 2.0f;        // 中心から輪郭への落ち方(大きいほど、中心だけが濃い)
inline const Vector3 kHaloColor{1.0f, 0.40f, 0.18f};
// 衝撃波のリング(Burst で、時間差で外へ広がる)
constexpr int kShockwaveCount = 3;        // リングの数(上限は kMaxShockwaves)
constexpr int kMaxShockwaves = 4;         // リングのモデルを、あらかじめ持つ数
constexpr float kShockwaveStagger = 0.12f; // リングごとの、始まりのずれ(Burst の進み具合の割合)
constexpr float kShockwaveGrow = 2.6f;    // リングが、最後に広がる大きさ(Peel の終わりの半径に対する倍率)
constexpr float kShockwaveSharpness = 14.0f; // リングの細さ(大きいほど、細い)
constexpr float kShockwaveAlpha = 0.95f;
inline const Vector3 kShockwaveColor{1.0f, 0.93f, 0.80f}; // リングの色(白に近い暖色)
// 爆散の白いフラッシュ(Burst の始まりに、画面全体が光って、すぐ晴れる)
constexpr float kFlashSeconds = 0.35f;
constexpr float kFlashPeakAlpha = 0.9f;
} // namespace BossExplosionDefaults

struct BossExplosionSettings {
  float startRadius = BossExplosionDefaults::kStartRadius;
  float maxRadius = BossExplosionDefaults::kMaxRadius;
  float burstGrow = BossExplosionDefaults::kBurstGrow;
  float rampShare = BossExplosionDefaults::kRampShare;
  float rimPower = BossExplosionDefaults::kRimPower;
  float patchStart = BossExplosionDefaults::kPatchStart;
  Vector3 rimColor = BossExplosionDefaults::kRimColor;
  float haloScale = BossExplosionDefaults::kHaloScale;
  float haloAlpha = BossExplosionDefaults::kHaloAlpha;
  float haloPower = BossExplosionDefaults::kHaloPower;
  Vector3 haloColor = BossExplosionDefaults::kHaloColor;
  int shockwaveCount = BossExplosionDefaults::kShockwaveCount;
  float shockwaveStagger = BossExplosionDefaults::kShockwaveStagger;
  float shockwaveGrow = BossExplosionDefaults::kShockwaveGrow;
  float shockwaveSharpness = BossExplosionDefaults::kShockwaveSharpness;
  float shockwaveAlpha = BossExplosionDefaults::kShockwaveAlpha;
  Vector3 shockwaveColor = BossExplosionDefaults::kShockwaveColor;
  StageImpactSettings flash{BossExplosionDefaults::kFlashSeconds, BossExplosionDefaults::kFlashPeakAlpha};
};

class BossExplosion {
public:
  // 球のモデルを読み込む。シーンの開始時に 1 回呼ぶ(読み込めなければ、球は出ない)
  void Initialize(const char *modelPath);

  // 演出を始める。shell: ボスの外殻のオブジェクト(ModelComponent を持つ)。crackShape: 球の中心(origin)を、ひび割れの形から取る
  void Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, const BossCrackShape &crackShape, const BossExplosionSettings &settings);

  // いまのサブフェーズと、その中の進み具合(0 〜 1)から、球・ハロ・リングの大きさと濃さを決める。毎フレーム呼ぶ(Begin のあと)
  // deltaTime: 1 フレームの実時間(秒。球面の斑が流れる位相と、フラッシュの経過に使う)
  void Update(ClearPhase phase, float phaseProgress, float deltaTime);

  // 球・ハロ・リングを描く。シーンの Draw の中で、物体を描いたあとに呼ぶ(濃さが 0 のものは、何もしない)
  void Draw(Renderer *renderer);

  // 爆散の白いフラッシュを描く。カメラのすぐ前の板なので、シーンの Draw の最後に、描画のカメラの位置と注視点を渡して呼ぶ
  void DrawFlash(Renderer *renderer, const Vector3 &eye, const Vector3 &target);

private:
  // 球のモデルを、中心と半径と色で置く
  static void PlaceSphere_(ModelInstance &model, const Vector3 &center, float radius, const Vector3 &color, float alpha);

  ModelInstance core_;       // 核(プラズマ球)
  ModelInstance halo_;       // ハロ(核の外側へにじむ光)
  std::array<ModelInstance, BossExplosionDefaults::kMaxShockwaves> shockwaves_; // 衝撃波のリング
  std::array<float, BossExplosionDefaults::kMaxShockwaves> shockwaveAlpha_{};   // リングごとの、いまの濃さ(0 のときは描かない)
  StageImpactEffects flash_; // 爆散の白いフラッシュ
  bool flashFired_ = false;  // フラッシュを、すでに出したか(Burst の最初の 1 回だけ)
  bool ready_ = false;       // モデルを読み込めたか
  std::shared_ptr<AbsoluteEngine::GameObject> shell_;
  BossCrackShape crackShape_{};
  BossExplosionSettings settings_;
  float coreAlpha_ = 0.0f;   // 核の、いまの濃さ(0 のときは描かない)
  float haloAlpha_ = 0.0f;   // ハロの、いまの濃さ(0 のときは描かない)
  float phase_ = 0.0f;       // 球面の斑が流れる位相(秒。実時間で進む)
};
