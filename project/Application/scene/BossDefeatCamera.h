#pragma once
#include "ClearDirector.h"
#include "Type/Vector.h"

// ボス撃破の演出のカメラワーク(ボスを、斜めに周回する)の、位置・注視点・画角を求めるクラス。
//
// 分けた理由: カメラの動きは、「サブフェーズごとに、どう動くか」という、まとまった責務を持つ。
// 時間の枠(ClearDirector)、ボスの見た目(BossDefeatVisual)とは別の部品にして、シーンは、結果の姿勢を受け取って、描画の間だけ適用する。
// 状態を持たない: レールカメラの、いまの位置と注視点を、毎フレーム受け取って、そこからの変位として求める(レールが動いても、付いてくる)。
//
// 動き(サブフェーズごと):
//   HitStop : 周回なし。短い揺れと、軽い画角のパンチ(撃破の衝撃)
//   Crack   : ボスの中心を軸に、周回を始める。少し寄る(距離 x kCrackDistanceScale へ)。注視点は、ボスの中心へ寄っていく
//   Peel    : 周回しつづけ、Peel の終わりで、約 kTotalAngleDegrees になる。距離は、元へ引く(球が膨らむので、全体が収まる)
//   Burst   : 周回は止まる。爆散の瞬間に、短い揺れと、軽い画角のパンチ
//   Flow    : 追う目的地(次の島)が無いときは、レールの位置と向きへ戻る(周回の角度、注視点が、元へ戻る)。
//             あるときは、戻らず、周回の終わりの位置のまま、向きだけが、ボスから島へ移っていく(粒の流れを追う)。
//             位置は、島全体が枠に収まる距離(島を含む球が、画角に収まる距離 x 余裕)より近ければ、その距離まで、島から離れる方向へ下がる(近づきはしない)
//   Release : 追う目的地があるときだけ。島全体が枠に収まった状態で、島の解放を見せる(ほんの少しだけ寄る)
//   Cover   : 追うときは、Release の終わりの位置のまま保つ(遷移の最中に、レールの位置へ戻らない)。追わないときは、Flow の終わり(レールの位置)のまま
//   それ以外 : 差し替えなし(レールカメラそのまま)
// 周回は、Crack と Peel を通して、始めと終わりがゆるやかな 1 つの曲線で進める(Crack と Peel の境目で、速さが跳ねない)。
// 高さは変えない(雲海や島にめり込みにくい)。差し替えは、描画の間だけ(ScopedCameraOverride)。ゲームの進行に使うカメラは、変えない。

namespace BossDefeatCameraDefaults {
constexpr float kTotalAngleDegrees = 90.0f;   // 周回の角度(Peel の終わりの値)
constexpr float kOrbitDirection = 1.0f;       // 回る向き: 1 = 左へ(ボスを正面から見て、カメラが左へ動く)、-1 = 右へ
constexpr float kViewFovY = 0.45f;            // ゲームカメラの縦の画角(ラジアン)。シーンが、実際の値で上書きする(GameScene の kGameCameraFovY)
constexpr float kFitMargin = 1.25f;           // 島を含む球が、枠に収まる距離に掛ける余裕(1 で、ぴったり。大きいほど、島の周りが空く)
constexpr float kChaseDollyRatio = 0.04f;     // Release の間に、さらに島へ寄る量(島を収める距離に対する割合。小さくして、全体が枠に収まったまま見せる)
constexpr float kCrackDistanceScale = 0.85f;  // Crack の終わりの、ボスまでの水平距離の倍率(元の距離に対する。Peel で 1 へ戻る)
// 衝撃の揺れと画角のパンチ(HitStop の始めと、Burst の始めの 2 回)
constexpr float kShakeSeconds = 0.30f;        // 揺れが収まるまでの時間(実時間。秒)
constexpr float kShakeAmplitude = 0.35f;      // 揺れの最大の大きさ(ワールド単位。カメラの位置と注視点を、同じだけずらす)
constexpr float kShakeFrequency = 26.0f;      // 揺れの速さ(1 秒あたりの往復)
constexpr float kPunchFovScale = 0.92f;       // 衝撃の瞬間の、画角の倍率(1 より小さい = ズームインしたように見える)。揺れと同じ時間で、1 へ戻る
} // namespace BossDefeatCameraDefaults

struct BossDefeatCameraSettings {
  float totalAngleDegrees = BossDefeatCameraDefaults::kTotalAngleDegrees;
  float orbitDirection = BossDefeatCameraDefaults::kOrbitDirection;
  float crackDistanceScale = BossDefeatCameraDefaults::kCrackDistanceScale;
  float viewFovY = BossDefeatCameraDefaults::kViewFovY;
  float fitMargin = BossDefeatCameraDefaults::kFitMargin;
  float chaseDollyRatio = BossDefeatCameraDefaults::kChaseDollyRatio;
  float shakeSeconds = BossDefeatCameraDefaults::kShakeSeconds;
  float shakeAmplitude = BossDefeatCameraDefaults::kShakeAmplitude;
  float shakeFrequency = BossDefeatCameraDefaults::kShakeFrequency;
  float punchFovScale = BossDefeatCameraDefaults::kPunchFovScale;
};

// 追う目的地(次の島)。カメラは、島全体を枠に収めて見せる
struct BossDefeatChase {
  Vector3 viewCenter{0.0f, 0.0f, 0.0f}; // 島の見た目の中心(向ける先。ワールド)
  float boundRadius = 0.0f;             // 島を含む球の半径(viewCenter から測る)
};

// 差し替えるカメラの姿勢
struct BossDefeatCameraPose {
  Vector3 eye{0.0f, 0.0f, 0.0f};
  Vector3 target{0.0f, 0.0f, 0.0f};
  float fovScale = 1.0f; // 通常の画角に掛ける倍率
};

class BossDefeatCamera {
public:
  // 差し替えるかを決める。演出の最中(HitStop から Flow まで)だけ true
  static bool IsActive(const ClearDirector &director);

  // カメラの姿勢を求める。
  // director: 撃破演出の時間(サブフェーズ、進み具合、経過)
  // baseEye / baseTarget: レールカメラの、いまの位置と注視点(差し替える前の値)
  // center: 周回の軸(ボスの中心のワールド位置。体の動きに付いてくる)
  // chase: 粒を追う目的地(次の島)。nullptr なら、追わず、Flow でレールへ戻る
  // settings: 動きの設定
  static BossDefeatCameraPose Compute(const ClearDirector &director, const Vector3 &baseEye, const Vector3 &baseTarget,
                                      const Vector3 &center, const BossDefeatChase *chase, const BossDefeatCameraSettings &settings);
};
