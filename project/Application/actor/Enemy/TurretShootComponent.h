#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include "EnemyShootUtil.h"
#include <string>

/// <summary>
/// 定点タレット型敵の攻撃コンポーネント。
/// EnemyShootComponentと違い自機は移動しないため、移動量から進行方向を
/// 推定する仕組み（facingDir_）を持たず、毎回プレイヤーへの方向を直接計算して狙う。
/// クールダウン経過後にburstCount_発をburstDelay_間隔で連射する。
/// </summary>
class TurretShootComponent : public AbsoluteEngine::IComponent {
public:
    TurretShootComponent() = default;
    ~TurretShootComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "TurretShootComponent"; }

private:
    bool CanFire(Vector3& outToPlayerDir) const;
    void FireOneBullet(const Vector3& dir);

    float shootInterval_ = 2.5f;  // バースト間のクールダウン
    float cooldownTimer_ = 0.0f;
    EnemyShootUtil::IslandHideGate islandGate_; // 島に隠れている間は撃たない。出てきてから少し待つ

    int burstCount_ = 3;           // 1バーストの発射数
    int burstShotsRemaining_ = 0;  // 現在のバーストで残っている発射数
    float burstDelay_ = 0.15f;     // バースト内の発射間隔
    float burstTimer_ = 0.0f;
};
