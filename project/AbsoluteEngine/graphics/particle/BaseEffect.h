#pragma once
#include "Transform.h"

class Camera;

class BaseEffect {
public:
    BaseEffect() = default;
    virtual ~BaseEffect() = default;

    // 派生クラスで初期化・更新・描画を実装する
    virtual void Initialize() {}
    virtual void Update(float deltaTime, const Camera* camera) = 0;
    virtual void Draw() = 0;

    bool IsDead() const { return isDead_; }
    void Kill() { isDead_ = true; }

    // 読み取り専用で取得する。書き換えは SetTransform 経由にする
    const Transform& GetTransform() const { return transform_; }
    void SetTransform(const Transform& transform) { transform_ = transform; }

protected:
    Transform transform_{};
    float lifetime_ = 1.0f;
    float age_ = 0.0f;
    bool isDead_ = false;
};
