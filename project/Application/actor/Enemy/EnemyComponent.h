#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include <functional>
#include <string>

/// <summary>
/// 敵が倒れた理由。onDestroyed の呼び出し側(GameScene)が、「自分で倒した」ときだけ報酬(スコア、撃破数、アイテム)を
/// 出すために、倒す側(弾、体当たり)が渡す。爆発などの演出は、どちらの理由でも出す。
/// </summary>
enum class EnemyDestroyReason {
    Player,       // 自機の弾(ロックオン弾を含む)で倒された
    SelfDestruct, // 自機への体当たりなどで、自分で壊れた(自機が倒したのではない)
};

/// <summary>
/// 敵の基本コンポーネント。
/// 死亡・ヒット時にコールバック（onDestroyed）を発火することで、
/// GameScene 等の上位層と疎結合なイベント通知（Observer）を実現する。
/// </summary>
class EnemyComponent : public AbsoluteEngine::IComponent {
public:
    EnemyComponent() = default;
    ~EnemyComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "EnemyComponent"; }

    bool IsActive() const { return isActive_; }
    void OnCollision(AbsoluteEngine::GameObject* other) override;
    // 弾が当たった時の処理（デフォルトは既存の雑魚敵と同じ1発必殺）。
    // reason は、倒れたときに onDestroyed へ渡す理由(弾は既定の Player。体当たりの自滅は SelfDestruct を渡す)
    void OnHit(int damage = 1, EnemyDestroyReason reason = EnemyDestroyReason::Player);

    bool IsDead() const { return isDead_; }

    float GetCollisionRadius() const { return radius_; }

    // 耐久力（タレット/ミニボス等、複数発で倒れる敵向け。デフォルトは1=既存の雑魚敵と同じ）
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // -----------------------------------------------------------------------
    // イベントコールバック（Observer）
    // -----------------------------------------------------------------------
    // 撃破された瞬間に1回だけ呼ばれる。
    // 引数は爆発座標（= 敵のワールド座標）と、倒れた理由。
    // GameScene 側で Spawn 時にラムダを登録しておくことで疎結合に演出を発火できる。
    std::function<void(const Vector3&, EnemyDestroyReason)> onDestroyed;

private:
    bool isActive_ = true;
    bool isDead_ = false;
    bool effectFired_ = false; // onDestroyed コールバックを既に発火済みか
    EnemyDestroyReason destroyReason_ = EnemyDestroyReason::Player; // 倒れた理由(倒れた瞬間に決まる)
    float radius_ = 2.0f;     // コライダー半径

    int hp_ = 1;
    int maxHp_ = 1;

    float dissolveTimer_ = 0.0f;
    float dissolveDuration_ = 1.0f; // 1秒かけてディゾルブ消滅

    // 被弾リアクション（即死しなかった時に一瞬白く光らせる）
    float hitFlashTimer_ = 0.0f;
    Vector4 colorBeforeFlash_{1.0f, 1.0f, 1.0f, 1.0f};
};
