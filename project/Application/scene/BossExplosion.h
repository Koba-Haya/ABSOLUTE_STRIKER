#pragma once
#include <memory>

#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Enemy/BossModelSpec.h"
#include "ClearDirector.h"
#include "Type/Vector.h"

class Renderer;

// ボス撃破の演出の、爆破の球(体の中心から広がる、光る半透明の球)を受け持つクラス。
//
// 分けた理由: 球は、「大きさと濃さの時間変化」「描き方(半透明の球のモデル)」という、まとまった責務を持つ。
// 光線(BossLightBeams)や、外殻のひび割れ(BossDefeatVisual)とは別の部品にして、BossDefeatVisual は、サブフェーズと進み具合を渡すだけにする。
//
// 見た目: 縁が濃い赤橙で、中心は透ける球の殻(ModelLightingMode::kRimGlow)。
// 球の中が透けるので、崩れていく殻や、金色の粒が、球の中に見える(球で全部を覆い隠さない)。
// 動き:
//   Peel  : 崩壊と同時に、体の中心から球が広がる(始めは速く、終わりへ向けてゆるやか)。濃さは、始めに立ち上がる
//   Burst : さらに少し広がりながら、薄れて消える
//   それ以外 : 描かない
// 使い方: Initialize(シーンの開始時に 1 回。モデルの読み込みを、演出の最中にしないため) -> Begin(演出の開始時) -> Update / Draw(毎フレーム)

namespace BossExplosionDefaults {
constexpr const char *kModelPath = "resources/app/sphere/sphere.obj"; // 半径 1 の球(exe 基準のパス)
constexpr float kStartRadius = 0.5f;     // Peel の始まりの半径(ワールド単位)
constexpr float kMaxRadius = 9.5f;       // Peel の終わりの半径(ボスは幅 約 9、全長 約 14)
constexpr float kBurstGrow = 1.25f;      // Burst の間に、さらに広がる倍率(Peel の終わりの半径に対する)
constexpr float kRampShare = 0.15f;      // Peel の始めの、濃さが立ち上がる割合(0 〜 1)
constexpr float kPeakAlpha = 0.9f;       // いちばん濃いときの透明度(縁の濃さ)
constexpr float kRimSharpness = 3.0f;    // 縁の細さ(大きいほど、縁の細い輪だけが濃くなり、中が透ける。小さいと、球全体が濃く、塗りつぶしに見える)
inline const Vector3 kRimColor{1.0f, 0.20f, 0.05f};    // 縁の色(赤橙。明るい空の上でも、白く飛ばない)
inline const Vector3 kCenterColor{1.0f, 0.72f, 0.40f}; // 中心の色(明るい橙)
} // namespace BossExplosionDefaults

struct BossExplosionSettings {
  float startRadius = BossExplosionDefaults::kStartRadius;
  float maxRadius = BossExplosionDefaults::kMaxRadius;
  float burstGrow = BossExplosionDefaults::kBurstGrow;
  float rampShare = BossExplosionDefaults::kRampShare;
  float peakAlpha = BossExplosionDefaults::kPeakAlpha;
  float rimSharpness = BossExplosionDefaults::kRimSharpness;
  Vector3 rimColor = BossExplosionDefaults::kRimColor;
  Vector3 centerColor = BossExplosionDefaults::kCenterColor;
};

class BossExplosion {
public:
  // 球のモデルを読み込む。シーンの開始時に 1 回呼ぶ(読み込めなければ、球は出ない)
  void Initialize(const char *modelPath);

  // 演出を始める。shell: ボスの外殻のオブジェクト(ModelComponent を持つ)。crackShape: 球の中心(origin)を、ひび割れの形から取る
  void Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, const BossCrackShape &crackShape, const BossExplosionSettings &settings);

  // いまのサブフェーズと、その中の進み具合(0 〜 1)から、球の大きさと濃さを決める。毎フレーム呼ぶ(Begin のあと)
  void Update(ClearPhase phase, float phaseProgress);

  // 球を描く。シーンの Draw の中で、物体を描いたあとに呼ぶ(濃さが 0 のときは、何もしない)
  void Draw(Renderer *renderer);

private:
  ModelInstance model_;
  bool ready_ = false;       // モデルを読み込めたか
  std::shared_ptr<AbsoluteEngine::GameObject> shell_;
  BossCrackShape crackShape_{};
  BossExplosionSettings settings_;
  float alpha_ = 0.0f;       // いまの濃さ(0 のときは描かない)
};
