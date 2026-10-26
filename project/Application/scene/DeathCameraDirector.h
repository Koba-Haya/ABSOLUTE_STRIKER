#pragma once
#include "Vector.h"
#include <array>

// 死亡演出のカメラ。レールとは独立に、蝶を中心にした極座標(方位、仰角、距離)と画角を、キーフレームの表で動かす。
// StageStartDirector と同じ作り: カメラ自体は動かさない。シーンが Update で時間を進め、GetPose の値を、描画の間だけカメラへ重ねる
// (ScopedCameraOverride)。ゲームの進行(レール、敵、当たり判定)が使うカメラの値は変わらない。
//
// カメラワーク(案 A: 回り込み、急降下、雲へ突入):
//   HitStop : 蝶へ少し寄って、画角を絞る
//   Crumble : 蝶を中心に、ゆっくり回り込みながら引く(破片が飛び散るのを見せる)
//   Fall    : 仰角を上げて、落ちる蝶を俯瞰気味に追う(下の雲海が迫る)
//   Impact  : ほぼ真上から、距離を詰めて、画角を広げる(急降下の速さ)
//   Cover   : 雲の中へ入り、白くなる。この間に、遷移の霧と水滴が画面を覆う
// 注視点は、蝶の位置を、少し遅れて追う(カメラマンが追いかける感じ)。

namespace DeathCameraDefaults {
// 1 つのキーフレーム: 時刻は、サブフェーズの終わり(PlayerDeathDirector::GetPhaseEndTime)に合わせる。
// 方位は、死亡の瞬間のカメラの方位からの回り込み(ラジアン。正で、蝶から見て反時計回りの側へ回る)、
// 仰角は水平からの角度(ラジアン)、距離は蝶からカメラまで(ユニット)、画角は通常の画角に掛ける倍率
struct Key {
  float azimuthDelta;
  float elevation;
  float distance;
  float fovScale;
};

// キーの数: 0 = 死亡の瞬間(カメラの実際の位置から作る。この表の値は使わない)、
// 1 = ヒットストップの終わり、2 = 崩壊の終わり、3 = 落下の終わり、4 = 着地の終わり、5 = 着地のあと kCoverHoldSeconds 秒後
constexpr int kKeyCount = 6;
constexpr float kCoverHoldSeconds = 2.0f; // 最後のキーの時刻 = 着地の終わり + この秒数(遷移の覆いの長さ)

constexpr Key kKeys[kKeyCount] = {
    {0.00f, 0.00f, 0.0f, 1.00f},   // 0: 死亡の瞬間(使わない)
    {0.00f, 0.00f, 20.5f, 0.90f},  // 1: 少し寄って、画角を絞る(仰角 0 = 死亡の瞬間の仰角のまま。下の kKeepElevation を参照)
    {0.70f, 0.35f, 28.0f, 1.00f},  // 2: 約 40 度回り込み、少し見下ろして、引く
    {1.20f, 0.90f, 17.0f, 1.10f},  // 3: 約 70 度、仰角約 50 度、落ちる蝶を俯瞰気味に追う
    {1.60f, 1.45f, 7.0f, 1.40f},   // 4: ほぼ真上から、近く、画角を広げる
    {1.90f, 1.50f, 2.5f, 1.60f},   // 5: 蝶に重なるほど近づき、雲の中へ
};
// キー 1 の仰角は、死亡の瞬間のカメラの仰角をそのまま使う(0 の値の代わりの印)
constexpr float kKeepElevation = 0.0f;

// 注視点が、蝶の位置へ追いつく速さ(1 秒あたり。大きいほど、遅れずに追う)
constexpr float kFollowSharpness = 7.0f;
// 破片が広がったとき、カメラを、本体と破片の両方が画面に入る距離まで引く割合(広がり 1 ユニットにつき、距離をこの分だけ足す)と、その上限(ユニット)
constexpr float kSpreadDistanceScale = 1.2f;
constexpr float kMaxSpreadDistance = 14.0f;
// 仰角の上限(真上は、カメラの上方向が定まらないので、少し手前で止める)
constexpr float kMaxElevation = 1.52f;
} // namespace DeathCameraDefaults

struct DeathCameraSettings {
  std::array<DeathCameraDefaults::Key, DeathCameraDefaults::kKeyCount> keys{};
  float coverHoldSeconds = DeathCameraDefaults::kCoverHoldSeconds;
  float followSharpness = DeathCameraDefaults::kFollowSharpness;
  float spreadDistanceScale = DeathCameraDefaults::kSpreadDistanceScale;
  float maxSpreadDistance = DeathCameraDefaults::kMaxSpreadDistance;

  DeathCameraSettings() {
    for (int i = 0; i < DeathCameraDefaults::kKeyCount; ++i) keys[i] = DeathCameraDefaults::kKeys[i];
  }
};

// 今の瞬間のカメラ
struct DeathCameraPose {
  Vector3 eye{0.0f, 0.0f, 0.0f};
  Vector3 target{0.0f, 0.0f, 1.0f};
  float fovScale = 1.0f; // 通常の画角への倍率
};

class DeathCameraDirector {
public:
  // 演出を始める。startEye / startTarget: 死亡の瞬間のカメラ。startFocus: 死亡の瞬間の蝶の位置。
  // keyTimes: 各キーの時刻(実時間。秒。DeathCameraDefaults::kKeyCount 個。[0] は 0)
  void Begin(const DeathCameraSettings& settings, const Vector3& startEye, const Vector3& startTarget, const Vector3& startFocus,
             const std::array<float, DeathCameraDefaults::kKeyCount>& keyTimes);
  void Reset() { active_ = false; }

  // 実時間の 1 フレーム分進める。focusPosition: 追いかける位置(ワールド。本体と破片の間)。
  // spread: 本体と破片の広がり(ユニット)。大きいほど、距離を引いて、両方を画面に収める
  void Update(float deltaTime, const Vector3& focusPosition, float spread = 0.0f);

  bool IsActive() const { return active_; }
  const DeathCameraPose& GetPose() const { return pose_; }

private:
  struct Spherical {
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float distance = 1.0f;
    float fovScale = 1.0f;
  };
  Spherical Evaluate_(float time) const;

  DeathCameraSettings settings_;
  std::array<float, DeathCameraDefaults::kKeyCount> keyTimes_{};
  Spherical start_;      // 死亡の瞬間のカメラの極座標
  Vector3 focus_{0.0f, 0.0f, 0.0f}; // 追いかけている注視点(蝶より少し遅れる)
  float elapsed_ = 0.0f;
  bool active_ = false;
  DeathCameraPose pose_;
};
