#pragma once
#include "GameCamera.h"
#include "Renderer.h"
#include "Vector.h"

// 描画の間だけ、カメラの位置・注視点・画角を差し替え、スコープを抜けるときに元へ戻すクラス。
// ステージ開始演出の俯瞰と、死亡演出のカメラが共通で使う。
// ゲームの進行(自機の位置、敵、弾)に使うカメラの値は変えず、見せるだけの差し替えにする。
// 元の位置へ戻すので、世界が動いている間も、ずれが毎フレーム積み重ならない(レール進行は、毎フレーム、カメラの位置を絶対値で決め直す)
class ScopedCameraOverride {
public:
  // camera: 差し替えるカメラ(nullptr ならなにもしない)。eye / target: 差し替える位置・注視点。fovY: 差し替える画角。
  // restoreFovY / nearZ / farZ: 戻すときの投影の設定(通常のゲームカメラの値)。画角の差し替えのときも、ニア・ファーは同じ値を使う
  ScopedCameraOverride(GameCamera *camera, const Vector3 &eye, const Vector3 &target, float fovY, float restoreFovY, float nearZ,
                       float farZ)
      : camera_(camera), restoreFovY_(restoreFovY), nearZ_(nearZ), farZ_(farZ) {
    if (camera_ == nullptr) {
      return;
    }
    eye_ = camera_->GetEye();
    target_ = camera_->GetTarget();
    camera_->SetEye(eye);
    camera_->SetTarget(target);
    camera_->SetPerspective(fovY, Renderer::GetInstance()->GetAspectRatio(), nearZ_, farZ_);
  }

  ~ScopedCameraOverride() {
    if (camera_ == nullptr) {
      return;
    }
    camera_->SetEye(eye_);
    camera_->SetTarget(target_);
    camera_->SetPerspective(restoreFovY_, Renderer::GetInstance()->GetAspectRatio(), nearZ_, farZ_);
  }

  ScopedCameraOverride(const ScopedCameraOverride &) = delete;
  ScopedCameraOverride &operator=(const ScopedCameraOverride &) = delete;

private:
  GameCamera *camera_ = nullptr;
  Vector3 eye_{0.0f, 0.0f, 0.0f};    // 差し替える前の位置(戻す先)
  Vector3 target_{0.0f, 0.0f, 1.0f}; // 差し替える前の注視点(戻す先)
  float restoreFovY_ = 0.45f;
  float nearZ_ = 0.1f;
  float farZ_ = 1000.0f;
};
