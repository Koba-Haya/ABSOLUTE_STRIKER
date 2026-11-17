#pragma once
#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "ClearDirector.h"
#include "Type/Vector.h"

class Renderer;

// 次の島の解放(Release)で、島の上に立つ光の柱と、根元に広がる光の輪を受け持つクラス。
//
// 分けた理由: 光の柱は、「時間による伸び方と濃さ」「描き方(球のモデルを引き伸ばした光)」というまとまった責務を持つ。
// 島の見た目(蘇りの波面)は NextIslandView が担当し、柱は、それと同じ Release の進み具合で動く別の部品にする。
//
// 見た目: 球のモデル(爆破の球と同じ)を、縦に細長く引き伸ばした光(ModelLightingMode::kSoftGlow。中心ほど濃く、縁へ向けて透ける)と、
//         根元に広がる、平たい球の縁の輪(kRimGlow。縁だけが濃い)。どちらも、アルファ合成のエフェクト用の描き方(DrawEffectModel)。
// 加算合成にしない理由: 明るい空や雲の上では、加算の光は白く飛んで、見えなくなるため(光線を、通常のアルファ合成にしたのと同じ理由)。
//         色を保つアルファ合成で、濃い金色にする。
// 柱は、球を細長くした形なので、根元が太く、先へ向けて細くなる(根元は島に埋まるので、島の上に出た半分が、柱になる)。
// 動き(Release の進み具合 p。0 〜 1):
//   0 〜 kRiseShare   : 柱が、根元から上へ伸びて立ち上がる。光の輪が広がり始める
//   そのあと          : 柱が、ゆるく脈打つ。島の色が蘇っていく(NextIslandView)
//   終わりの kFadeShare : 柱が薄れて消える
//   Release 以外      : 描かない
// 使い方: Initialize(シーンの開始時に 1 回) -> Begin(柱の根元) -> Update / Draw(毎フレーム)

namespace IslandReleasePillarDefaults {
constexpr const char *kModelPath = "resources/app/bossBurst/sphere.obj"; // 半径 1 の球(爆破の球と同じ。テクスチャは使わない)
// 柱
constexpr float kPillarRadius = 3.0f;      // 柱の根元の半径(ワールド単位。島の半径は 約 19)
constexpr float kPillarHeight = 60.0f;     // 柱の高さ(島の上面の中心から。球の半分の高さ)
constexpr float kPillarGlowPower = 1.2f;   // 柱の縁の透け方(大きいほど、中心だけが濃く、縁が細い)
constexpr float kRiseShare = 0.25f;        // Release のうち、柱が立ち上がる割合(0 〜 1。約 0.5 秒)
constexpr float kFadeShare = 0.25f;        // Release の終わりの、柱が薄れる割合
constexpr float kPeakAlpha = 0.9f;         // 柱の最大の濃さ
constexpr float kPulseAmount = 0.12f;      // 脈打つ大きさ(濃さに対する割合)
constexpr float kPulseCycles = 3.0f;       // Release の間に脈打つ回数
inline const Vector3 kPillarColor{1.0f, 0.78f, 0.25f}; // 柱の色(濃い金色。明るい空の上でも、色が残る)
// 光の輪(根元から広がる、平たい球の縁)
constexpr float kRingStartRadius = 3.0f;   // 輪の始まりの半径
constexpr float kRingEndRadius = 28.0f;    // 輪の終わりの半径(島の上面の半径 約 19 を越える)
constexpr float kRingThickness = 1.8f;     // 輪の厚み(球を、縦に潰した半径)
constexpr float kRingSharpness = 5.0f;     // 輪の縁の細さ(大きいほど、細い輪)
constexpr float kRingShare = 0.55f;        // Release のうち、輪が広がる割合
constexpr float kRingPeakAlpha = 0.95f;
inline const Vector3 kRingColor{1.0f, 0.85f, 0.45f}; // 輪の色
constexpr float kMinRiseRatio = 0.02f;     // 柱の立ち上がりの割合の下限(これ以下は、描かない。潰れた行列を作らないため)
} // namespace IslandReleasePillarDefaults

class IslandReleasePillar {
public:
  // 球のモデルを読み込む。シーンの開始時に 1 回呼ぶ(読み込めなければ、柱は出ない)
  void Initialize();

  // 柱の根元(島の上面の中心。ワールド)を決める。演出の開始時に呼ぶ
  void Begin(const Vector3 &base);

  // Release の進み具合から、柱と輪の大きさと濃さを決める。毎フレーム呼ぶ
  void Update(ClearPhase phase, float phaseProgress);

  // 柱と輪を描く。物体を描いたあとに呼ぶ(濃さが 0 のときは、何もしない)
  void Draw(Renderer *renderer);

private:
  ModelInstance pillar_; // 柱(kSoftGlow)
  ModelInstance ring_;   // 光の輪(kRimGlow)
  Vector3 base_{0.0f, 0.0f, 0.0f};
  bool ready_ = false;
  bool pillarVisible_ = false; // いま、柱を描くか
  bool ringVisible_ = false;   // いま、輪を描くか
};
