#include "MainCameraView.h"

#include "GameCamera.h"
#include "Method.h" // Multiply, MakeIdentity4x4

namespace AbsoluteEngine {

namespace {
// 画面内判定で、カメラ背後(クリップ空間の w が 0 以下)とみなす下限。以前の各射撃コンポーネントと同じ値
constexpr float kMinClipW = 0.01f;
// カメラが無いときに返す位置・向き
constexpr Vector3 kNoCameraVector = {0.0f, 0.0f, 0.0f};
} // namespace

bool MainCameraView::HasCamera() const { return camera_ != nullptr; }

bool MainCameraView::IsOnScreen(const Vector3& worldPos, float ndcMargin) const {
  if (!camera_) return false;

  const Matrix4x4 vpMat = Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());

  // 世界座標をクリップ空間に変換する(行ベクトル x 行列。w は奥行きに相当する)
  const float w = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];
  if (w <= kMinClipW) return false; // カメラの背後

  const float ndcX = (worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0]) / w;
  const float ndcY = (worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1]) / w;

  return ndcX >= -ndcMargin && ndcX <= ndcMargin && ndcY >= -ndcMargin && ndcY <= ndcMargin;
}

Vector3 MainCameraView::GetEye() const { return camera_ ? camera_->GetEye() : kNoCameraVector; }
Vector3 MainCameraView::GetForward() const { return camera_ ? camera_->GetForward() : kNoCameraVector; }
Vector3 MainCameraView::GetRight() const { return camera_ ? camera_->GetRight() : kNoCameraVector; }
Vector3 MainCameraView::GetActualUp() const { return camera_ ? camera_->GetActualUp() : kNoCameraVector; }

Matrix4x4 MainCameraView::GetViewMatrix() const { return camera_ ? camera_->GetViewMatrix() : MakeIdentity4x4(); }
Matrix4x4 MainCameraView::GetProjectionMatrix() const { return camera_ ? camera_->GetProjectionMatrix() : MakeIdentity4x4(); }

void MainCameraView::SetPose(const Vector3& eye, const Vector3& target, const Vector3& up) {
  if (!camera_) return;
  camera_->SetEye(eye);
  camera_->SetTarget(target);
  camera_->SetUp(up);
}

} // namespace AbsoluteEngine
