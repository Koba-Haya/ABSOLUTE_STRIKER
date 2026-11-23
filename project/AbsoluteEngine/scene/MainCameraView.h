#pragma once
#include <memory>

#include "ICameraRig.h"
#include "IScreenView.h"

class GameCamera;

namespace AbsoluteEngine {

// メインカメラを保持する側(BaseScene)の持ち物を、IScreenView と ICameraRig の 2 つの窓口として見せるクラス。
//
// 分けた理由: カメラ(GameCamera)自身に窓口を実装させると、カメラが設定される前に作られた部品が、窓口を受け取れない。
// また、メインカメラが差し替えられたとき、部品が持つ窓口が古いカメラを指してしまう。
// このクラスは、BaseScene が持つ shared_ptr への「参照」を持ち、問い合わせのたびに、今のメインカメラを見る。
// そのため、カメラの設定前からあり、設定や差し替えに追従する。カメラが無いときは、安全な値を返す(IScreenView を参照)。
class MainCameraView final : public IScreenView, public ICameraRig {
public:
  // camera: メインカメラを保持する shared_ptr への参照。参照先(BaseScene のメンバ)が、このクラスより長く生きること
  explicit MainCameraView(const std::shared_ptr<GameCamera>& camera) : camera_(camera) {}

  // IScreenView と ICameraRig の両方で同じ名前・同じ意味のため、1 つの実装が両方を満たす
  bool HasCamera() const override;

  // IScreenView
  bool IsOnScreen(const Vector3& worldPos, float ndcMargin) const override;
  Vector3 GetEye() const override;
  Vector3 GetForward() const override;
  Vector3 GetRight() const override;
  Vector3 GetActualUp() const override;
  Matrix4x4 GetViewMatrix() const override;
  Matrix4x4 GetProjectionMatrix() const override;

  // ICameraRig
  void SetPose(const Vector3& eye, const Vector3& target, const Vector3& up) override;

private:
  const std::shared_ptr<GameCamera>& camera_; // 参照(所有しない)。今のメインカメラ
};

} // namespace AbsoluteEngine
