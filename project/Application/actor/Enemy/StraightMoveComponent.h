#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/camera/ITimelineCamera.h"
#include "Type/Vector.h"
#include <string>

class StraightMoveComponent : public AbsoluteEngine::IComponent {
public:
    StraightMoveComponent() = default;
    ~StraightMoveComponent() override = default;

    void Update(float deltaTime) override;

    // レール座標の指定で出現したとき、移動の向きを「レールの進行方向の逆(自機へ近づく向き)」にする。
    // 世界座標の指定で出現したときは呼ばれず、既定の向き(世界の -Z)のまま動く
    void OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) override;

    std::string GetTypeName() const override { return "StraightMoveComponent"; }

    // 移動速度の設定・取得
    void SetSpeed(float speed) { speed_ = speed; }
    float GetSpeed() const { return speed_; }

private:
    float speed_ = 10.0f; // デフォルトの移動速度

    // 移動の向き(長さ 1)。既定は世界の -Z(従来の動き)。レール座標で出現したときは、レールの進行方向の逆になる
    Vector3 direction_ = { 0.0f, 0.0f, -1.0f };
};
