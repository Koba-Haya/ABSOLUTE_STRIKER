#include "SpeedStreaks.h"

#include "AbsoluteEngine/resources/AssetManager.h"
#include "BasisMatrix.h"
#include "ModelResource.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>

namespace {
// ライティングモード 0: ライトの影響を受けず、テクスチャの色をそのまま出す
constexpr int32_t kUnlitMode = 0;
constexpr float kTwoPi = 6.2831853f;
// 筋の格子: 進行方向の周期(ユニット)。カメラの前方、この距離の間に、筋を置く
constexpr float kPeriodAhead = 60.0f;
// 筋を、進行方向の軸のまわりのどの半径に置くか(ユニット)。中心を空けて、真ん中にいる竜に重ならないようにする
constexpr float kMinRadius = 3.5f;
constexpr float kMaxRadius = 11.0f;
// 画面は横長なので、上下の広がりを、左右に対する割合で狭める
constexpr float kVerticalRatio = 0.7f;
// 板の半分の長さ・半分の幅(ユニット)の範囲。長さ方向が進行方向
constexpr float kMinHalfLength = 3.0f;
constexpr float kMaxHalfLength = 8.0f;
constexpr float kMinHalfWidth = 0.04f;
constexpr float kMaxHalfWidth = 0.09f;
// 筋ごとの濃さの、最大に対する割合の範囲(筋によって濃さを変える)
constexpr float kMinAlphaRatio = 0.4f;
// 進行方向の端(出現・消滅)で、筋を薄くする距離(ユニット)
constexpr float kEdgeFade = 12.0f;
// カメラに近すぎる筋を消す距離(ユニット)。画面を横切る太い白い棒に見えるのを避ける
constexpr float kNearFadeStart = 3.0f;
constexpr float kNearFadeEnd = 9.0f;
// 強さがこれ以下なら、描かない
constexpr float kMinStrength = 0.01f;
// 長さがこれ以下のベクトルは、向きが決められないので、正規化しない
constexpr float kMinDirectionLength = 1e-4f;

// 筋の番号と用途から、0 〜 1 の値を作る(同じ番号・用途なら、いつも同じ値。状態を持たない)
float Hash01(uint32_t index, uint32_t salt) {
  uint32_t h = index * 374761393u + salt * 668265263u;
  h = (h ^ (h >> 13u)) * 1274126177u;
  h ^= h >> 16u;
  return static_cast<float>(h) * (1.0f / 4294967295.0f);
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }
float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
// 0 〜 1 を、始まりと終わりがなめらかな S 字に変える
float Smooth01(float t) { return t * t * (3.0f - 2.0f * t); }
float PositiveMod(float a, float period) {
  const float m = std::fmod(a, period);
  return m < 0.0f ? m + period : m;
}
} // namespace

void SpeedStreaks::Initialize(const char *modelPath) {
  auto resource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(modelPath);
  if (!resource) {
    return;
  }
  ModelInstance::CreateInfo info{};
  info.resource = resource;
  info.lightingMode = kUnlitMode;
  ready_ = true;
  for (ModelInstance &instance : instances_) {
    ready_ = instance.Initialize(info) && ready_;
  }
}

void SpeedStreaks::Draw(Renderer *renderer, const Vector3 &eye, const Vector3 &axis, float strength, const SpeedStreakSettings &settings) {
  if (!ready_ || strength <= kMinStrength) {
    return;
  }

  // 進行方向(筋の長さの向き)と、それに垂直な右・上
  const Vector3 flowAxis = Normalize(axis);
  Vector3 axisRight = Cross({0.0f, 1.0f, 0.0f}, flowAxis);
  axisRight = Length(axisRight) > kMinDirectionLength ? Normalize(axisRight) : Vector3{1.0f, 0.0f, 0.0f};
  const Vector3 axisUp = Cross(flowAxis, axisRight);

  // カメラの位置を、(右, 上, 進行方向)の座標で表す。筋は、この座標の格子の上に置く
  const float eyeRight = Dot(eye, axisRight);
  const float eyeUp = Dot(eye, axisUp);
  const float eyeAhead = Dot(eye, flowAxis);

  const float stretch = 1.0f + (settings.maxStretch - 1.0f) * strength;

  for (int i = 0; i < kCount; ++i) {
    const uint32_t id = static_cast<uint32_t>(i);

    // 進行方向の位置: ワールドに固定した周期の格子のうち、カメラの前方 kPeriodAhead 以内の位置。
    // カメラが進むと、同じ筋が後ろへ流れていき、周期の端で次の筋が前方に現れる
    const float ahead = PositiveMod(Hash01(id, 3) * kPeriodAhead - eyeAhead, kPeriodAhead);

    // 軸のまわりの位置: 角度と半径で決める(カメラの軸のまわりに、放射状に散らす)
    const float angle = Hash01(id, 1) * kTwoPi;
    const float radius = Lerp(kMinRadius, kMaxRadius, Hash01(id, 2));
    const float right = eyeRight + std::cos(angle) * radius;
    const float up = eyeUp + std::sin(angle) * radius * kVerticalRatio;

    const Vector3 pos{axisRight.x * right + axisUp.x * up + flowAxis.x * (eyeAhead + ahead),
                      axisRight.y * right + axisUp.y * up + flowAxis.y * (eyeAhead + ahead),
                      axisRight.z * right + axisUp.z * up + flowAxis.z * (eyeAhead + ahead)};

    // 濃さ: 筋ごとの違い x 強さ。進行方向の端と、カメラに近すぎる所で、薄くする
    const float edge = (std::min)(Smooth01(Clamp01(ahead / kEdgeFade)), Smooth01(Clamp01((kPeriodAhead - ahead) / kEdgeFade)));
    const Vector3 toCamera{eye.x - pos.x, eye.y - pos.y, eye.z - pos.z};
    const float distance = Length(toCamera);
    const float nearFade = Smooth01(Clamp01((distance - kNearFadeStart) / (kNearFadeEnd - kNearFadeStart)));
    const float alpha = settings.maxAlpha * Lerp(kMinAlphaRatio, 1.0f, Hash01(id, 6)) * strength * edge * nearFade;
    if (alpha <= 0.0f) {
      continue;
    }

    // 筋の板は、長さ方向(進行方向)を保ったまま、カメラへ向ける(軸まわりのビルボード)
    Vector3 plateRight = Cross(flowAxis, toCamera);
    plateRight = Length(plateRight) > kMinDirectionLength ? Normalize(plateRight) : Vector3{1.0f, 0.0f, 0.0f};
    const Vector3 plateNormal = Cross(plateRight, flowAxis);
    const float halfLength = Lerp(kMinHalfLength, kMaxHalfLength, Hash01(id, 4)) * stretch;
    const float halfWidth = Lerp(kMinHalfWidth, kMaxHalfWidth, Hash01(id, 5));

    ModelInstance &instance = instances_[static_cast<size_t>(i)];
    instance.SetWorld(MakeBasisMatrix({plateRight.x * halfWidth, plateRight.y * halfWidth, plateRight.z * halfWidth},
                                      {flowAxis.x * halfLength, flowAxis.y * halfLength, flowAxis.z * halfLength}, plateNormal, pos));
    instance.SetColor({1.0f, 1.0f, 1.0f, alpha});
    renderer->DrawEffectModel(&instance);
  }
}
