#pragma once
#include <memory>

#include "ClearDirector.h"
#include "IslandReleasePillar.h"
#include "StageIsland.h"
#include "Type/Vector.h"

class Renderer;

// ボス撃破の演出で、粒が飛んでいく先の、次のステージをイメージする仮の島(ボスの前方遠くに出る浮島)を受け持つクラス。
//
// 分けた理由: 仮の島は、「どこに置くか」「いつ現れるか」「粒とカメラの目的地」という、まとまった責務を持つ。
// 島 1 つの見た目の持ち物(モデルと行列)は、既存の StageIsland を使い、ここでは、その置き場所と出し方だけを決める。
// 粒(BossDustHold)とカメラ(BossDefeatCamera)は、ここが教える目的地(GetTargetPoint)へ向かう。
//
// 置き場所: ボスの中心から、レールの進行方向へ kAheadDistance、右へ kLateralOffset、上へ kHeightOffset。
//   進行方向は、演出を始める時点のレールカメラの向き(水平)から取る。ステージ(ボス)ごとに調整できるよう、設定にまとめてある
// 出方: Peel の間に、小さい所から、元の大きさへ育つ(粒が着くころには、そこにある)。Peel より前は描かない。
//       はじめは「くすんだ色」。Release で、島の上に光の柱(IslandReleasePillar)が立ち、柱の根元から、蘇りの波面が島の表面を広がって、生きた色になる
//       (島の色は、専用のシェーダー IslandRevive.PS.hlsl で描く)
// 使い方: Initialize(シーンの開始時に 1 回) -> Begin(演出の開始時。解放するときだけ) -> Update / Draw(毎フレーム)

namespace NextIslandDefaults {
constexpr const char *kModelPath = "resources/app/island/island_forest.gltf"; // 島のモデル(既存のステージの飾りの島と同じ。exe 基準のパス)
constexpr float kAheadDistance = 100.0f;   // ボスの前方(レールの進行方向)への距離
constexpr float kLateralOffset = 18.0f;    // 右へのずれ(負は左。ユニット)
constexpr float kHeightOffset = -4.0f;     // 高さのずれ(島の上面の中心の、ボスの中心からの高さ。負は下)
constexpr float kScale = 6.0f;             // 島の大きさの倍率(ステージの島の最大と同じ)
constexpr float kYaw = 0.6f;               // 島の向き(ラジアン。見た目に変化を付けるだけ)
constexpr float kTopHeightPerScale = 0.6f; // 島の上面の中心から、粒の目的地(頂点、木の高さあたり)までの高さ(倍率 1 のとき)
// カメラが島全体を枠に収めるための、島の見た目の大きさ(倍率 1 のとき)。島は、上面から下へ、岩が伸びた形(上面の半径 約 3.2、上へ 約 0.6、下へ 約 3.6 〜 6)
constexpr float kViewCenterDropPerScale = 2.0f; // 島の見た目の中心の、上面の中心からの下がり(倍率 1 あたり)
constexpr float kBoundRadiusPerScale = 4.5f;    // 島を含む球の半径(倍率 1 あたり。見た目の中心から測る)
// 蘇りの波面(モデル本来の単位。島の上面の半径は 約 3.2、下の先端は 約 6 まで伸びる)
constexpr float kReviveNoiseCell = 1.2f;    // 波面の縁の乱れ(ノイズ)の 1 マスの大きさ。小さいほど、縁が細かく乱れる
constexpr float kReviveFrontWidth = 0.9f;   // 波面の縁の幅(この幅の中で、金色に光りながら、色が変わる)
constexpr float kReviveSpread = 7.0f;       // 波面が最後に届く距離(中心から、島の最も遠い所まで)
constexpr float kReviveGlowIntensity = 1.6f; // 縁の光の強さ
inline const Vector3 kReviveGlowColor{1.0f, 0.86f, 0.45f}; // 縁の光の色(金色)
constexpr float kMinScaleRatio = 0.05f;    // 育ち始めの、大きさの割合(0 だと、行列が潰れるため、わずかに残す)
constexpr float kMinForwardLength = 1e-4f; // 長さがこれ以下のベクトルは、向きが決められない
} // namespace NextIslandDefaults

struct NextIslandSettings {
  float aheadDistance = NextIslandDefaults::kAheadDistance;
  float lateralOffset = NextIslandDefaults::kLateralOffset;
  float heightOffset = NextIslandDefaults::kHeightOffset;
  float scale = NextIslandDefaults::kScale;
  float yaw = NextIslandDefaults::kYaw;
};

class NextIslandView {
public:
  // 島のモデルを読み込む。シーンの開始時に 1 回呼ぶ(演出の最中に、読み込みで画面が止まらないように)。読めなければ、島は出ない
  void Initialize(const char *modelPath);

  // 島を置く(解放するときだけ呼ぶ。呼ばなければ、島は出ない)。
  // bossCenter: ボスの中心(ワールド)。cameraEye / cameraTarget: レールカメラの位置と注視点(進行方向を、水平の向きから取る)
  void Begin(const Vector3 &bossCenter, const Vector3 &cameraEye, const Vector3 &cameraTarget, const NextIslandSettings &settings);

  // 演出を止めて、島を消す(シーンの終わり、次の演出の前)
  void Reset() { active_ = false; scaleRatio_ = 0.0f; }

  // いまのサブフェーズと進み具合から、島の大きさ(育ち具合)を決める。毎フレーム呼ぶ
  void Update(ClearPhase phase, float phaseProgress);

  // 島を描く。雲の依頼のあと、物体の描画の前に呼ぶ(雲の合成が深度を見るので、島の下側は雲に埋まる。ステージの島と同じ)
  void Draw(Renderer *renderer);

  // 光の柱と輪を描く。物体を描いたあと(半透明なので、島や雲の合成のあと)に呼ぶ
  void DrawEffects(Renderer *renderer);

  // 島を出す演出か(Begin のあと)
  bool IsActive() const { return active_; }

  // 粒の目的地(島の上面の少し上。ワールド)
  const Vector3 &GetTargetPoint() const { return targetPoint_; }

  // カメラが島全体を収めて見せるための、島の見た目の中心(岩の下側を含めた、島の真ん中あたり。ワールド)と、島を含む球の半径
  const Vector3 &GetViewCenter() const { return viewCenter_; }
  float GetBoundRadius() const { return boundRadius_; }

private:
  StageIsland island_;
  bool ready_ = false;   // モデルを読み込めたか
  bool active_ = false;  // 演出で島を出すか
  NextIslandSettings settings_;
  Vector3 position_{0.0f, 0.0f, 0.0f};
  Vector3 targetPoint_{0.0f, 0.0f, 0.0f};
  Vector3 viewCenter_{0.0f, 0.0f, 0.0f};
  float boundRadius_ = 0.0f;
  float scaleRatio_ = 0.0f; // 島の大きさの割合(0 〜 1。Peel で育つ)
  float reviveRatio_ = 0.0f; // 蘇った割合(0 〜 1。Release で、柱が立ったあと、波面が広がる)
  IslandReleasePillar pillar_; // Release で島の上に立つ光の柱と、根元に広がる輪
};
