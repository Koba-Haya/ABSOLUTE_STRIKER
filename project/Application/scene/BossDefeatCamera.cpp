#include "BossDefeatCamera.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kDegreesToRadians = kPi / 180.0f;
constexpr float kTwoPi = 2.0f * kPi;
// 揺れの 3 軸の位相のずれ(軸ごとに違う値にして、3 軸が同じ波にならないようにする)
constexpr float kShakePhaseY = 1.7f;
constexpr float kShakePhaseZ = 3.1f;
// 揺れの縦と奥行きの大きさ(横に対する割合。横揺れを主にする)
constexpr float kShakeYRatio = 0.7f;
constexpr float kShakeZRatio = 0.4f;
// 追う向き(ボスの中心から島への水平の距離)の下限。これ以下は、向きが決められないので、追わない
constexpr float kMinChaseLength = 1e-3f;

float Saturate(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}

// 0 〜 1 のなめらかな補間(始まりと終わりがゆるやか)
float Smooth01(float t) {
  t = Saturate(t);
  return t * t * (3.0f - 2.0f * t);
}

float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

Vector3 Lerp(const Vector3 &a, const Vector3 &b, float t) {
  return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t)};
}

// 衝撃からの経過 t(秒)での、揺れの強さ(1 から 0 へ。2 乗で、始めに強く、すぐ収まる)。範囲外は 0
float ShakeEnvelope(float t, float seconds) {
  if (t < 0.0f || seconds <= 0.0f || t >= seconds) return 0.0f;
  const float remain = 1.0f - t / seconds;
  return remain * remain;
}
} // namespace

bool BossDefeatCamera::IsActive(const ClearDirector &director) {
  const ClearPhase phase = director.GetPhase();
  return phase == ClearPhase::HitStop || phase == ClearPhase::Crack || phase == ClearPhase::Peel || phase == ClearPhase::Burst ||
         phase == ClearPhase::Flow || phase == ClearPhase::Release || phase == ClearPhase::Cover;
}

BossDefeatCameraPose BossDefeatCamera::Compute(const ClearDirector &director, const Vector3 &baseEye, const Vector3 &baseTarget,
                                               const Vector3 &center, const BossDefeatChase *chase,
                                               const BossDefeatCameraSettings &settings) {
  // Cover(クリア画面への遷移の最中。スキップで直接来たときも)は、追うときは、島の解放が終わった位置のまま保つ(Release の終わりと同じ姿勢。
  // ここで差し替えをやめると、レールカメラへ飛び戻ってから、遷移してしまう)。追わないときは、Flow の終わり(= レールの位置)と同じで、差し替えても変わらない
  const ClearPhase rawPhase = director.GetPhase();
  const ClearPhase phase = (rawPhase == ClearPhase::Cover && chase) ? ClearPhase::Release : rawPhase;
  const float p = rawPhase == ClearPhase::Cover ? 1.0f : Saturate(director.GetPhaseProgress());
  const float elapsed = director.GetElapsed();

  // 周回の割合(0 = 元の位置、1 = 周回の終わり)、水平距離の倍率、注視点がボスの中心へ寄る割合
  float angleFraction = 0.0f;
  float distanceScale = 1.0f;
  float aimBlend = 0.0f;

  // Crack と Peel を通した進み具合(0 〜 1)。周回は、この間を、1 つのなめらかな曲線で進む
  const float spanStart = director.GetPhaseEndTime(ClearPhase::HitStop);
  const float spanEnd = director.GetPhaseEndTime(ClearPhase::Peel);
  const float spanProgress = spanEnd > spanStart ? Saturate((elapsed - spanStart) / (spanEnd - spanStart)) : 1.0f;

  switch (phase) {
  case ClearPhase::Crack:
    angleFraction = Smooth01(spanProgress);
    distanceScale = Lerp(1.0f, settings.crackDistanceScale, Smooth01(p)); // 寄る
    aimBlend = Smooth01(p);
    break;
  case ClearPhase::Peel:
    angleFraction = Smooth01(spanProgress);
    distanceScale = Lerp(settings.crackDistanceScale, 1.0f, Smooth01(p)); // 元の距離へ引く
    aimBlend = 1.0f;
    break;
  case ClearPhase::Burst:
    angleFraction = 1.0f;
    aimBlend = 1.0f;
    break;
  case ClearPhase::Flow:
    // 追わないときは、レールの位置と向きへ戻る。追うときは、周回の終わりの姿勢から始める(下で、島へ向かう動きを重ねる)
    angleFraction = chase ? 1.0f : 1.0f - Smooth01(p);
    aimBlend = chase ? 1.0f : 1.0f - Smooth01(p);
    break;
  case ClearPhase::Release:
    // 追うときだけ存在するサブフェーズ。周回の終わりの姿勢から始める(下で、島を向く動きを重ねる)
    angleFraction = 1.0f;
    aimBlend = 1.0f;
    break;
  default:
    break; // HitStop、そのほか: 周回なし
  }

  // ボスの中心を軸に、水平に回す(高さは変えない)。左手系・y 上: 角度が正のとき、カメラは、ボスを正面から見て左へ動く
  const float angle = settings.orbitDirection * settings.totalAngleDegrees * kDegreesToRadians * angleFraction;
  const float cosA = std::cos(angle);
  const float sinA = std::sin(angle);
  const float offsetX = baseEye.x - center.x;
  const float offsetY = baseEye.y - center.y;
  const float offsetZ = baseEye.z - center.z;
  BossDefeatCameraPose pose;
  pose.eye = {center.x + (offsetX * cosA + offsetZ * sinA) * distanceScale, center.y + offsetY,
              center.z + (-offsetX * sinA + offsetZ * cosA) * distanceScale};
  pose.target = Lerp(baseTarget, center, aimBlend);

  // 粒を追う(Flow、Release): 位置は、周回の終わりのまま。向き(注視点)だけを、ボスの中心から島の見た目の中心へ移す(粒の流れを追う)。
  // ただし、島全体が枠に収まる距離(島を含む球が、縦の画角に収まる距離 x 余裕)より近いときは、島から離れる方向へ、その距離まで下がる(近づきはしない)。
  // 下がる向きは、島の見た目の中心から、周回の終わりのカメラへ向かう水平の向き。高さは、周回の終わりのまま
  if (chase && (phase == ClearPhase::Flow || phase == ClearPhase::Release)) {
    const Vector3 orbitEye = pose.eye; // 周回の終わりの位置(Flow と Release では、周回の角度が最大で、距離は元のまま)
    const float halfFovTan = std::tan(settings.viewFovY * 0.5f);
    const float fitDistance = halfFovTan > kMinChaseLength ? chase->boundRadius * settings.fitMargin / halfFovTan : 0.0f;
    Vector3 away{orbitEye.x - chase->viewCenter.x, 0.0f, orbitEye.z - chase->viewCenter.z};
    const float horizontal = std::sqrt(away.x * away.x + away.z * away.z);
    Vector3 stopEye = orbitEye;
    if (horizontal > kMinChaseLength) {
      away = {away.x / horizontal, 0.0f, away.z / horizontal};
      // 高さの差を引いた、水平の距離(見た目の中心までの直線距離が fitDistance になる水平距離)。いまより近づかない
      const float dy = orbitEye.y - chase->viewCenter.y;
      const float fitHorizontal = std::sqrt((std::max)(fitDistance * fitDistance - dy * dy, 0.0f));
      const float stopHorizontal = (std::max)(horizontal, fitHorizontal);
      stopEye = {chase->viewCenter.x + away.x * stopHorizontal, orbitEye.y, chase->viewCenter.z + away.z * stopHorizontal};
      if (phase == ClearPhase::Release) {
        // 島全体が枠に収まったまま、ほんの少しだけ、島へ寄る
        const float dolly = stopHorizontal * settings.chaseDollyRatio * Smooth01(p);
        stopEye = {stopEye.x - away.x * dolly, stopEye.y, stopEye.z - away.z * dolly};
      }
    }
    if (phase == ClearPhase::Flow) {
      const float f = Smooth01(p);
      pose.eye = Lerp(orbitEye, stopEye, f);
      pose.target = Lerp(center, chase->viewCenter, f);
    } else {
      pose.eye = stopEye;
      pose.target = chase->viewCenter;
    }
  }

  // 衝撃の揺れと画角のパンチ: HitStop の始め(経過 0)と、爆散の瞬間(Burst の始め)の 2 回。実時間で動かす
  const float burstStart = director.GetPhaseEndTime(ClearPhase::Peel);
  const float envelope = (std::max)(ShakeEnvelope(elapsed, settings.shakeSeconds), ShakeEnvelope(elapsed - burstStart, settings.shakeSeconds));
  if (envelope > 0.0f) {
    const float wave = elapsed * settings.shakeFrequency * kTwoPi;
    const float amplitude = settings.shakeAmplitude * envelope;
    // カメラの位置と注視点を、同じだけずらす(向きは変えず、画面全体が揺れる)
    const Vector3 shake{std::sin(wave) * amplitude, std::sin(wave + kShakePhaseY) * amplitude * kShakeYRatio,
                        std::sin(wave + kShakePhaseZ) * amplitude * kShakeZRatio};
    pose.eye = {pose.eye.x + shake.x, pose.eye.y + shake.y, pose.eye.z + shake.z};
    pose.target = {pose.target.x + shake.x, pose.target.y + shake.y, pose.target.z + shake.z};
    pose.fovScale = Lerp(1.0f, settings.punchFovScale, envelope);
  }
  return pose;
}
