#pragma once
#include "Type/Vector.h"
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"

class BulletComponent : public AbsoluteEngine::IComponent {
public:
  BulletComponent() = default;
  ~BulletComponent() override = default;

  void Initialize(const Vector3 &vel);
  void Update(float deltaTime) override;
  void OnCollision(AbsoluteEngine::GameObject* other) override;
  
  std::string GetTypeName() const override { return "BulletComponent"; }

  bool IsActive() const { return isActive_; }
  void Deactivate() { isActive_ = false; }
  float GetCollisionRadius() const { return radius_; }

  // 自機の通常弾で撃ち落とせる弾か(敵弾用)。既定は true
  bool CanBeShotDown() const { return canBeShotDown_; }
  void SetCanBeShotDown(bool canBeShotDown) { canBeShotDown_ = canBeShotDown; }

private:
  Vector3 velocity_{ 0, 0, 0 };
  bool isActive_ = false;
  float lifeTimer_ = 0.0f;
  float maxLife_ = 3.0f; // 3秒で消滅
  float radius_ = 0.2f;  // 当たり判定半径
  bool canBeShotDown_ = true; // 通常弾で撃ち落とせるか(敵弾にだけ意味がある)
};
