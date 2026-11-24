#include "GameCamera.h"
#include "Input.h"
#include "Method.h"

void GameCamera::SetController(std::unique_ptr<ICameraController> controller, const CameraContext& ctx) {
    if (controller_) {
        controller_->OnExit(*this, ctx);
    }
    controller_ = std::move(controller);
    if (controller_) {
        controller_->OnEnter(*this, ctx);
    }
}

void GameCamera::Update(const Input& /*input*/) {
    if (controller_) {
        controller_->Update(*this, ctx_);
    }
    RecomputeView_();
}

void GameCamera::RecomputeView_() {
    SetViewMatrix(MakeLookAtMatrix(eye_, target_, up_));
}

Vector3 GameCamera::GetForward() const {
    Vector3 diff = { target_.x - eye_.x, target_.y - eye_.y, target_.z - eye_.z };
    return Normalize(diff);
}

Vector3 GameCamera::GetRight() const {
    Vector3 forward = GetForward();
    return Normalize(Cross(up_, forward));
}

Vector3 GameCamera::GetActualUp() const {
    Vector3 forward = GetForward();
    Vector3 right = GetRight();
    return Normalize(Cross(forward, right));
}