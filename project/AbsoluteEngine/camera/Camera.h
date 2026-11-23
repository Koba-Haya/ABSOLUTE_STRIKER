#pragma once

#include "Method.h"

class Input;

/// <summary>
/// View / Projection を提供するカメラ基底
/// Update() は派生側が実装（DebugCameraAdapter / GameCamera など）
/// Projection は SetPerspective() で設定
/// </summary>
class Camera {
public:
    virtual ~Camera() = default;

    virtual void Initialize() {}
    virtual void Update(const Input& input) = 0;

    const Matrix4x4& GetViewMatrix() const { return view_; }
    const Matrix4x4& GetProjectionMatrix() const { return projection_; }

    void SetPerspective(float fovY, float aspect, float nearZ, float farZ) {
        projection_ = MakePerspectiveFovMatrix(fovY, aspect, nearZ, farZ);
    }

protected:
    // 派生クラスが、毎フレーム計算した View 行列を渡すための専用関数。
    // メンバ変数そのものは private に隠し、派生クラスから直接書き換えられないようにする
    void SetViewMatrix(const Matrix4x4& view) { view_ = view; }

private:
    Matrix4x4 view_ = MakeIdentity4x4();
    Matrix4x4 projection_ = MakeIdentity4x4();
};