#include "StageImpactEffects.h"

#include "AbsoluteEngine/resources/AssetManager.h"
#include "BasisMatrix.h"
#include "ModelResource.h"
#include "Renderer.h"

#include <cmath>

namespace {
// ライティングモード 0: ライトの影響を受けず、テクスチャの色をそのまま出す
constexpr int32_t kUnlitMode = 0;
// フラッシュの板を、カメラの前へ出す距離(ユニット)と、板の半分の幅。
// 板のモデルは、x・y が -1 〜 1 の四角形なので、半分の幅 1.6 は、距離 1.0 で、画角の外まで届く大きさ(タイトルのフラッシュと同じ値)
constexpr float kFlashDistance = 1.0f;
constexpr float kFlashHalfWidth = 1.6f;
// 長さがこれ以下のベクトルは、向きが決められないので、正規化しない
constexpr float kMinDirectionLength = 1e-4f;
} // namespace

void StageImpactEffects::Initialize(const char *flashModelPath) {
  auto resource = AbsoluteEngine::AssetManager::GetInstance()->Load<ModelResource>(flashModelPath);
  if (!resource) {
    return;
  }
  ModelInstance::CreateInfo info{};
  info.resource = resource;
  info.lightingMode = kUnlitMode;
  flashReady_ = flashModel_.Initialize(info);
}

void StageImpactEffects::Trigger(const StageImpactSettings &settings) {
  settings_ = settings;
  elapsed_ = 0.0f;
  triggered_ = true;
}

void StageImpactEffects::Update(float deltaTime) {
  if (!triggered_) {
    return;
  }
  elapsed_ += deltaTime;
  // フラッシュが終わったら、経過を数えるのをやめる
  if (elapsed_ >= settings_.flashSeconds) {
    triggered_ = false;
  }
}

void StageImpactEffects::DrawFlash(Renderer *renderer, const Vector3 &eye, const Vector3 &target) {
  if (!flashReady_ || !triggered_ || elapsed_ >= settings_.flashSeconds || settings_.flashSeconds <= 0.0f) {
    return;
  }

  // 最も濃い状態から、だんだん薄くなる(2 次曲線で、はじめに濃く、すぐ薄れる)
  const float remaining = 1.0f - elapsed_ / settings_.flashSeconds;
  const float alpha = settings_.flashPeakAlpha * remaining * remaining;

  // カメラの前方向・右・上(左手系: 上 x 前 = 右、前 x 右 = 上)に、板を向ける
  const Vector3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
  Vector3 right = Cross({0.0f, 1.0f, 0.0f}, forward);
  right = Length(right) > kMinDirectionLength ? Normalize(right) : Vector3{1.0f, 0.0f, 0.0f};
  const Vector3 up = Cross(forward, right);

  const Vector3 center{eye.x + forward.x * kFlashDistance, eye.y + forward.y * kFlashDistance, eye.z + forward.z * kFlashDistance};
  flashModel_.SetWorld(MakeBasisMatrix({right.x * kFlashHalfWidth, right.y * kFlashHalfWidth, right.z * kFlashHalfWidth},
                                       {up.x * kFlashHalfWidth, up.y * kFlashHalfWidth, up.z * kFlashHalfWidth}, forward, center));
  flashModel_.SetColor({1.0f, 1.0f, 1.0f, alpha});

  // 深度を書き込む(雲は、深度を見て後から描かれるので、書かないと、白い板の上に雲が重なってしまう)
  renderer->DrawEffectModel(&flashModel_, true);
}
