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
///
/// 設定(プレハブの JSON で調整できる。初期値は下の名前付き定数):
///   shootInterval : バースト間のクールダウン(秒)
///   burstCount    : 1バーストの発射数
///   burstDelay    : バースト内の発射間隔(秒)
///   bulletSpeed   : 弾の速さ(ユニット/秒)。自機の進み(レールの速さ)より十分速くする
///   fireRange     : 自機との距離がこれ以内のときだけ撃つ(ユニット)。0 以下なら、距離の制限なし。
///                   弾が届かない遠さから撃って、弾が無駄になる(届く前に通り過ぎる)のを防ぐ
///   leadAim       : true なら、自機の動く先を読んで撃つ(自機がレールに沿って前へ進むぶん、先へ撃つ)。
///                   false なら、今の自機の位置を狙う(自機が進むので、横にずれた敵の弾は、後ろを通り過ぎる)
/// </summary>
class TurretShootComponent : public AbsoluteEngine::IComponent {
public:
    TurretShootComponent() = default;
    ~TurretShootComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "TurretShootComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    bool CanFire(Vector3& outToPlayerDir) const;
    void FireOneBullet(const Vector3& dir);
    // 自機の速度(位置の変化から求める)を、毎フレーム更新する。動く先を読む(leadAim)のに使う
    void TrackPlayerVelocity(float deltaTime);
    // 弾の速さ bulletSpeed_ で、動いている自機に当たる向き(長さ1)を求める。解が無いときは false
    bool CalcLeadDirection(const Vector3& turretPos, const Vector3& playerPos, Vector3& outDir) const;

    // ---- 設定 ----
    float shootInterval_ = 2.5f;  // バースト間のクールダウン
    int burstCount_ = 3;           // 1バーストの発射数
    float burstDelay_ = 0.25f;     // バースト内の発射間隔
    float bulletSpeed_ = 30.0f;    // 弾の速さ
    float fireRange_ = 90.0f;      // この距離以内で撃つ(0 以下で制限なし)
    bool leadAim_ = true;          // 自機の動く先を読んで撃つか

    // ---- 状態 ----
    float cooldownTimer_ = 0.0f;
    EnemyShootUtil::IslandHideGate islandGate_; // 島に隠れている間は撃たない。出てきてから少し待つ
    int burstShotsRemaining_ = 0;  // 現在のバーストで残っている発射数
    float burstTimer_ = 0.0f;
    Vector3 prevPlayerPos_ = { 0.0f, 0.0f, 0.0f };  // 前フレームの自機の位置
    Vector3 playerVelocity_ = { 0.0f, 0.0f, 0.0f }; // 自機の速度の推定(少しならして、ぶれを抑える)
    bool hasPrevPlayerPos_ = false;
};
