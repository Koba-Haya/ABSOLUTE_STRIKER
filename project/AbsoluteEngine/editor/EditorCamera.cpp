#include "EditorCamera.h"
#include <numbers>
#include <algorithm>

namespace AbsoluteEngine {

void EditorCamera::Initialize() {
    SetViewMatrix(MakeIdentity4x4());
}

void EditorCamera::Update(const Input& input) {
    // ======================
    // 平行移動
    // ======================
    Vector3 move = { 0.0f, 0.0f, 0.0f };

    // キーボード移動（ローカル座標系）。requireMouseForMovement_がtrueの場合は
    // 右クリック中のみ有効（シーン編集中の誤操作防止）、falseなら常時有効
    const bool canKeyMove = !requireMouseForMovement_ || input.IsMouseDown(1);
    if (canKeyMove) {
        if (input.PressKey(DIK_W)) { move.z += keyMoveSpeed_; }
        if (input.PressKey(DIK_S)) { move.z -= keyMoveSpeed_; }
        if (input.PressKey(DIK_A)) { move.x -= keyMoveSpeed_; }
        if (input.PressKey(DIK_D)) { move.x += keyMoveSpeed_; }
        if (input.PressKey(DIK_Q)) { move.y += keyMoveSpeed_; }
        if (input.PressKey(DIK_E)) { move.y -= keyMoveSpeed_; }
    }

    auto mouse = input.GetMouse();

    // 中ボタン押下中：ドラッグでパン（平行移動）
    if (input.IsMouseDown(2)) {
        move.x += -mouse.dx * mousePanSpeed_;
        move.y +=  mouse.dy * mousePanSpeed_; // エディタカメラはY軸移動も有効化
    }

    // ホイール前後移動（ローカル Z）
    if (mouse.wheel != 0) {
        move.z += mouse.wheel * wheelMoveSpeed_;
    }

    // ローカル移動を回転でワールド方向へ
    move = TransformNormal(move, matRot_);
    translate_ = Add(translate_, move);

    // ======================
    // 回転
    // ======================
    const float rotSpeedKey = 0.05f;
    const float rotSpeedMouse = 0.005f;

    // 右ドラッグ：Yaw, Pitch 回転
    if (input.IsMouseDown(1)) {
        yaw_ += mouse.dx * rotSpeedMouse;   // これが横回転
        pitch_ += mouse.dy * rotSpeedMouse; // これが縦回転
    }

    // 矢印キー/Z/Cキーでの回転（有効化されている場合のみ）
    if (keyboardRotationEnabled_) {
        if (input.PressKey(DIK_LEFT))  { yaw_   -= rotSpeedKey; }
        if (input.PressKey(DIK_RIGHT)) { yaw_   += rotSpeedKey; }
        if (input.PressKey(DIK_UP))    { pitch_ -= rotSpeedKey; }
        if (input.PressKey(DIK_DOWN))  { pitch_ += rotSpeedKey; }
        if (input.PressKey(DIK_Z))     { roll_  += rotSpeedKey; }
        if (input.PressKey(DIK_C))     { roll_  -= rotSpeedKey; }
    }

    const float limit = (std::numbers::pi_v<float> * 0.5f) - 0.001f;
    pitch_ = std::clamp(pitch_, -limit, +limit);

    Matrix4x4 rotY = MakeRotateYMatrix(yaw_);
    Matrix4x4 rotX = MakeRotateXMatrix(pitch_);
    Matrix4x4 rotZ = MakeRotateZMatrix(roll_);
    matRot_ = Multiply(rotZ, Multiply(rotX, rotY));

    // ======================
    // View 行列更新
    // ======================
    UpdateMatrix();
}

void EditorCamera::UpdateMatrix() {
    // 以前はここで translate_ を matRot_ でもう一度変換していたが、translate_ は
    // Update() の移動加算で既にワールド座標として蓄積されているため、二重変換になっていた
    // （右ドラッグで視点を回すと、蓄積済みのtranslate_が新しい回転で再変換され、
    //   カメラが原点を中心に振れるように動いて見える不具合の原因だった）
    Vector3 cameraPosition = translate_;
    cameraPosition.x += shakeOffset_.x;
    cameraPosition.y += shakeOffset_.y;
    cameraPosition.z += shakeOffset_.z;
    Matrix4x4 translateMatrix = MakeTranslateMatrix(cameraPosition);
    Matrix4x4 worldMatrix = Multiply(matRot_, translateMatrix);
    SetViewMatrix(Inverse(worldMatrix));
}

void EditorCamera::FocusOn(const Vector3& targetPosition, float distance) {
    // 現在のカメラの向き（matRot_）のZ軸（前方向）ベクトルを取得
    Vector3 forward = { matRot_.m[2][0], matRot_.m[2][1], matRot_.m[2][2] };
    
    // ターゲットから forward 方向に distance だけ手前に引いた位置をカメラ位置にする
    translate_ = { targetPosition.x - forward.x * distance,
                   targetPosition.y - forward.y * distance,
                   targetPosition.z - forward.z * distance };
    
    // View行列の再計算
    UpdateMatrix();
}

} // namespace AbsoluteEngine
