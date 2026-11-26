#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "IEnemyDefeatObserver.h"
#include "Type/Vector.h"
#include <string>
#include <vector>

/// <summary>
/// 敵の基本コンポーネント(HP、被弾、倒れたあとの消え方)。
/// 倒れたことを、登録された受け取る側(IEnemyDefeatObserver)の全員へ知らせる(Observer パターンの Subject)。
/// 敵は、倒れたあとに何が起きるか(スコア、アイテム、演出)を知らない。受け取る側が、自分の処理を書く。
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
    // reason は、倒れたときに受け取る側へ渡す理由(弾は既定の Player。体当たりの自滅は SelfDestruct を渡す)
    void OnHit(int damage = 1, EnemyDestroyReason reason = EnemyDestroyReason::Player);

    bool IsDead() const { return isDead_; }

    float GetCollisionRadius() const { return radius_; }

    // 耐久力（タレット/ミニボス等、複数発で倒れる敵向け。デフォルトは1=既存の雑魚敵と同じ）
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // -----------------------------------------------------------------------
    // 倒れたことの通知(Observer)
    // -----------------------------------------------------------------------
    // 倒れたことを受け取る側を登録する。倒れた次の更新で、登録した全員の OnEnemyDefeated が 1 回ずつ呼ばれる。
    // 所有しない。observer は、この敵より長く生きるか、先に RemoveObserver すること。
    // 戻り値: 新しく登録したら true。nullptr、または登録済みなら、何もせず false(同じ受け取る側へ 2 回知らせないため)
    bool AddObserver(IEnemyDefeatObserver* observer);
    // 登録を外す(登録されていなければ何もしない)
    void RemoveObserver(IEnemyDefeatObserver* observer);

private:
    bool isActive_ = true;
    bool isDead_ = false;
    bool effectFired_ = false; // 倒れたことを、受け取る側へ知らせ済みか
    std::vector<IEnemyDefeatObserver*> observers_; // 倒れたことを受け取る側(所有しない)
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
