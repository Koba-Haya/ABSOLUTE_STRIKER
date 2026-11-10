#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/camera/ITimelineCamera.h"
#include "Type/Vector.h"
#include "EnemyShootUtil.h"
#include <string>

class EnemyShootComponent : public AbsoluteEngine::IComponent {
public:
    EnemyShootComponent() = default;
    ~EnemyShootComponent() override = default;

    void Update(float deltaTime) override;

    // レール座標の指定で出現したとき、進行方向の初期値を「レールの進行方向の逆(自機側を向く)」にする。
    // 動かない敵は、位置の変化から向きを推定できないので、この初期値のまま「正面の自機だけを撃つ」判定に使う。
    // 世界座標の指定で出現したときは呼ばれず、既定の向き(世界の -Z)のまま
    void OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) override;

    std::string GetTypeName() const override { return "EnemyShootComponent"; }

    // 発射間隔の設定
    void SetShootInterval(float interval) { shootInterval_ = interval; }
    float GetShootInterval() const { return shootInterval_; }

    // ゲームシーン側から発射タイミングを確認（ポーリング）するための機能
    bool WantToShoot() const { return wantToShoot_; }
    void ClearShootFlag() { wantToShoot_ = false; }

private:
    float shootInterval_ = 2.0f; // デフォルトは2秒に1回
    float shootTimer_ = 0.0f;
    EnemyShootUtil::IslandHideGate islandGate_; // 島に隠れている間は撃たない。出てきてから少し待つ
    bool wantToShoot_ = false;

    // 進行方向追跞用（マジックナンバー不要で動的に取得）
    Vector3 prevPos_     = { 0, 0, 0 }; // 前フレームの位置
    Vector3 facingDir_   = { 0, 0, -1 }; // 現在の進行方向（初期値：Z軍手前）
    bool hasPrevPos_     = false; // prevPos_が初期化済みかのフラグ
};
