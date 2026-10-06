#include "TitleDirector.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 6.2831853f;

// カット構成
constexpr float kShotDurationSec = 6.0f;
constexpr int kShotCount = 4;

// 自機の動き
constexpr float kFlightSpeed = 24.0f;   // 前進速度(unit/s)
constexpr float kWeaveAmplitude = 3.0f; // 左右の振れ幅
constexpr float kWeavePeriodSec = 6.0f;
constexpr float kBaseHeight = 1.5f;     // 水面からの高さ
constexpr float kBobAmplitude = 0.2f;   // 上下の揺れ
constexpr float kBobPeriodSec = 4.3f;
constexpr float kBankGain = 0.15f;      // 左右速度 1 あたりのバンク角(rad)
const Vector3 kPlayerScale{1.0f, 1.0f, 1.0f}; // dragon.obj は頭から尾まで 2.8 に揃えて書き出してあり、機首が +Z(エンジン空間)
constexpr int kRollShotIndex = 2;       // バレルロールするカット(③)
constexpr float kRollStartRatio = 0.2f; // カット内でロールを始める割合
constexpr float kRollEndRatio = 0.8f;

// スタート演出: 自機が u^2 で加速して前へ飛び去り、カメラは自機の動きの一部だけ付いていく(引き)。画角も広がる
constexpr float kLaunchDistance = 70.0f;       // 演出中に自機が余分に進む距離
constexpr float kLaunchCameraFollow = 0.35f;   // カメラが余分な移動のうち付いていく割合(残りは引きになる)
const Vector3 kLaunchCameraOffsetStart{0.0f, 1.2f, -9.0f};
const Vector3 kLaunchCameraOffsetEnd{0.0f, 2.4f, -18.0f};
constexpr float kLaunchFovGain = 0.3f;         // 画角(rad)の増加量

// カメラ
constexpr float kBaseFovY = 0.45f;
constexpr float kMinCameraHeight = 0.4f; // 水面との交点計算が破綻しない下限(水面より下は不可)
const Vector3 kWorldUp{0.0f, 1.0f, 0.0f};

// ロゴ・PRESS START(カメラ空間: x=右, y=上, z=前)。板ポリは横幅 2 が基準なので scale が半幅
// 下の寸法は基準距離 10 での値。実際はカメラのすぐ前(kOverlayDistance)に置き、同じ画面サイズになるよう縮める。
// 自機はカメラから常に 4 以上離れるので、深度テストでもロゴが必ず自機より手前になる
constexpr float kOverlayReferenceDistance = 10.0f;
constexpr float kOverlayDistance = 3.0f;
constexpr float kLogoHalfWidth = 3.0f;
constexpr float kLogoOffsetY = 1.4f;
constexpr float kStartHalfWidth = 1.5f;
constexpr float kStartOffsetY = -1.4f;
// スタート演出でのスライドアウト(基準距離 10 での移動量)と、かける時間の割合(演出の序盤)。
// 画面の上端・下端は基準距離で約 ±2.3、ロゴの半高さ約 0.75、PRESS START の半高さ約 0.38 なので、画面外へ出る量にしてある
constexpr float kLogoSlideOut = 2.6f;
constexpr float kStartSlideOut = 2.0f;
constexpr float kLaunchSlideRatio = 0.4f;

// 白フラッシュの板(横幅 2 基準・16:9)。カメラの目の前に置き、どの画角でも画面全体を覆うよう大きめにする
constexpr float kFlashDistance = 1.0f;
constexpr float kFlashHalfWidth = 1.6f;

Vector3 Plus(const Vector3 &a, const Vector3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Minus(const Vector3 &a, const Vector3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 Times(const Vector3 &v, float s) { return {v.x * s, v.y * s, v.z * s}; }
float Smooth01(float u) { return u * u * (3.0f - 2.0f * u); }
float Clamp01(float v) { return (std::min)((std::max)(v, 0.0f), 1.0f); }

// 自機の位置。時間の連続な式なので、ループの継ぎ目でも飛ばない
Vector3 PlayerPosition(float time) {
  const float weave = kWeaveAmplitude * std::sin(kTwoPi * time / kWeavePeriodSec);
  const float bob = kBobAmplitude * std::sin(kTwoPi * time / kBobPeriodSec);
  return {weave, kBaseHeight + bob, kFlightSpeed * time};
}

// 自機の左右方向の速度(位置の微分)
float PlayerLateralVelocity(float time) {
  const float omega = kTwoPi / kWeavePeriodSec;
  return kWeaveAmplitude * omega * std::cos(omega * time);
}

// 行を (右, 上, 前, 位置) として並べた行列。モデルの x→右, y→上, z→前 に対応する
Matrix4x4 MakeBasisMatrix(const Vector3 &right, const Vector3 &up, const Vector3 &forward, const Vector3 &position) {
  Matrix4x4 m = MakeIdentity4x4();
  m.m[0][0] = right.x;    m.m[0][1] = right.y;    m.m[0][2] = right.z;
  m.m[1][0] = up.x;       m.m[1][1] = up.y;       m.m[1][2] = up.z;
  m.m[2][0] = forward.x;  m.m[2][1] = forward.y;  m.m[2][2] = forward.z;
  m.m[3][0] = position.x; m.m[3][1] = position.y; m.m[3][2] = position.z;
  return m;
}

struct CameraShot {
  Vector3 eye;
  Vector3 target;
};

// カット別のカメラ。playerNow: 現在の自機位置, playerAtShotStart: カット開始時の自機位置, u: カット内の進行(0〜1)
CameraShot EvaluateShot(int shot, const Vector3 &playerNow, const Vector3 &playerAtShotStart, float u) {
  const float e = Smooth01(u);
  switch (shot) {
  case 0: {
    // ①ローアングル: 自機の進路の脇・前方に置いた固定カメラ。自機が奥から手前へ通過する
    const Vector3 eye = Plus(playerAtShotStart, Vector3{9.0f, -1.0f, 60.0f});
    return {eye, playerNow};
  }
  case 1: {
    // ②後方追従: 自機の後ろから、少し近づきながら追う
    const Vector3 offset = Lerp(Vector3{0.0f, 1.2f, -9.0f}, Vector3{0.0f, 0.8f, -6.0f}, e);
    return {Plus(playerNow, offset), Plus(playerNow, Vector3{0.0f, 0.3f, 6.0f})};
  }
  case 2: {
    // ③横トラッキング: 自機の右横から並走
    const Vector3 offset = Lerp(Vector3{9.0f, 0.6f, -2.0f}, Vector3{7.0f, 0.5f, 3.0f}, e);
    return {Plus(playerNow, offset), playerNow};
  }
  default: {
    // ④回り込み: 後方から自機の前へ半周する
    const float angle = kPi * e;
    const float radius = 8.0f + (6.0f - 8.0f) * e;
    const float height = 1.0f + 0.3f * e;
    const Vector3 offset{radius * std::sin(angle), height, -radius * std::cos(angle)};
    return {Plus(playerNow, offset), playerNow};
  }
  }
}
} // namespace

TitleState TitleDirector::Evaluate(float time, float launchProgress) const {
  const bool launching = launchProgress >= 0.0f;
  const float launchU = Clamp01(launchProgress);
  // 加速: 進行度の 2 乗で距離が伸びる(はじめはゆっくり、だんだん速くなる)
  const float launchBoost = launching ? launchU * launchU * kLaunchDistance : 0.0f;

  const float loopSec = kShotDurationSec * kShotCount;
  const float loopTime = std::fmod(time, loopSec);
  const int shot = static_cast<int>(loopTime / kShotDurationSec);
  const float timeInShot = loopTime - shot * kShotDurationSec;
  const float u = timeInShot / kShotDurationSec;
  // 自機は時間の連続な式なので、ループさせず絶対時間のまま使う(継ぎ目で飛ばない)
  const float shotStartTime = time - timeInShot;

  TitleState state{};

  // --- 自機 ---
  Vector3 playerPos = PlayerPosition(time);
  playerPos.z += launchBoost;
  const float vx = PlayerLateralVelocity(time);
  const float heading = std::atan2(vx, kFlightSpeed); // 進行方向(+Z から左右へ振れる角)
  const float bank = -kBankGain * vx;                 // 進行方向側の翼を下げる(Z 回転は正で右翼が上がる)
  float roll = 0.0f;
  if (!launching && shot == kRollShotIndex) {
    roll = kTwoPi * Smooth01(Clamp01((u - kRollStartRatio) / (kRollEndRatio - kRollStartRatio)));
  }
  // 行ベクトル規約なので左から順に適用: 機首軸(Z)まわりのロール → 進行方向(Y)
  const Matrix4x4 rotation = Multiply(MakeRotateZMatrix(bank + roll), MakeRotateYMatrix(heading));
  state.playerWorld = Multiply(Multiply(MakeScaleMatrix(kPlayerScale), rotation), MakeTranslateMatrix(playerPos));

  // --- カメラ ---
  CameraShot cam;
  state.fovY = kBaseFovY;
  if (launching) {
    // スタート演出のカメラ: 自機の余分な移動の一部しか追わないので、自機が離れていく。後ろへ引いて少し上がる
    const float e = Smooth01(launchU);
    Vector3 follow = PlayerPosition(time);
    follow.z += launchBoost * kLaunchCameraFollow;
    cam = {Plus(follow, Lerp(kLaunchCameraOffsetStart, kLaunchCameraOffsetEnd, e)), playerPos};
    state.fovY += kLaunchFovGain * e;
  } else {
    cam = EvaluateShot(shot, playerPos, PlayerPosition(shotStartTime), u);
  }
  state.cameraEye = cam.eye;
  state.cameraEye.y = (std::max)(state.cameraEye.y, kMinCameraHeight);
  state.cameraTarget = cam.target;

  // --- ロゴ・PRESS START: カメラの基底(右・上・前)に取り付ける ---
  const Vector3 forward = Normalize(Minus(state.cameraTarget, state.cameraEye));
  const Vector3 right = Normalize(Cross(kWorldUp, forward)); // 左手系: 上 × 前 = 右
  const Vector3 up = Cross(forward, right);
  // 画角が広がっても画面上の大きさと位置が変わらないよう、tan(fov/2) の比で補正する(補正しないと遠ざかって見える)
  const float fovScale = std::tan(state.fovY * 0.5f) / std::tan(kBaseFovY * 0.5f);
  const float overlayScale = kOverlayDistance / kOverlayReferenceDistance * fovScale;
  // スタート演出の序盤: ロゴは上へ、PRESS START は下へ画面の外までスライドして消える
  const float slide = launching ? Smooth01(Clamp01(launchU / kLaunchSlideRatio)) : 0.0f;
  const float logoOffsetY = kLogoOffsetY + kLogoSlideOut * slide;
  const float startOffsetY = kStartOffsetY - kStartSlideOut * slide;
  const Vector3 overlayCenter = Plus(state.cameraEye, Times(forward, kOverlayDistance));
  state.logoWorld = MakeBasisMatrix(Times(right, kLogoHalfWidth * overlayScale), Times(up, kLogoHalfWidth * overlayScale), forward, Plus(overlayCenter, Times(up, logoOffsetY * overlayScale)));
  state.startWorld = MakeBasisMatrix(Times(right, kStartHalfWidth * overlayScale), Times(up, kStartHalfWidth * overlayScale), forward, Plus(overlayCenter, Times(up, startOffsetY * overlayScale)));
  state.flashWorld = MakeBasisMatrix(Times(right, kFlashHalfWidth), Times(up, kFlashHalfWidth), forward, Plus(state.cameraEye, Times(forward, kFlashDistance)));
  return state;
}
