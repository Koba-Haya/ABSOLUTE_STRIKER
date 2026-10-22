#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

// ボスの攻撃パターン。文字列比較ではなくenumで管理する
// （先輩フィードバックで「AIの攻撃パターン選択を文字列比較で行っている」ことが
//   ✕評価の実例として挙がっていたため、この作品では避ける）。
enum class BossPhase {
    Phase1, // HP 100%〜67%: 自機狙いの単発弾のみ
    Phase2, // HP 67%〜34%: 単発弾 + 扇状弾（回避を要求）
    Phase3, // HP 34%〜0% : 高頻度の単発弾 + 拡大した扇状弾（弾幕）
};

class BossComponent : public AbsoluteEngine::IComponent {
public:
    BossComponent() = default;
    ~BossComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "BossComponent"; }

    bool IsActive() const { return isActive_; }
    void OnCollision(AbsoluteEngine::GameObject* other) override;
    void TakeDamage(int damage);

    float GetCollisionRadius() const { return radius_; }

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    BossPhase GetPhase() const { return phase_; }

private:
    void UpdatePhase();
    void UpdateAttack(float deltaTime);
    // カメラ(=レール)の前方に留まりつつ左右上下にゆっくり揺れる。自機の位置には影響されない
    void UpdateMovement(float deltaTime);

    // アクティブシーンから自機のGameObjectを探す（Fire*で使用）
    std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer() const;

    // 自機へ向かう単発弾を1発発射する
    void FireAimedShot(float speed);
    // 自機方向を中心に扇状の弾をcount発発射する（Z軸周りにspreadAngleDeg度ずつ振り分ける）
    void FireSpreadShot(int count, float spreadAngleDeg, float speed);

    bool isActive_ = true;
    float radius_ = 5.0f;  // ボスなので当たり判定を大きめに
    int hp_ = 40;          // ボスのHP（3フェーズ分の耐久を持たせる）
    int maxHp_ = 40;

    BossPhase phase_ = BossPhase::Phase1;

    float aimedShotTimer_ = 0.0f;
    float spreadShotTimer_ = 0.0f;

    // 存在をアピールするための緩やかな自転（静止した箱に見えないようにする程度の演出）
    float spinTimer_ = 0.0f;
    // 左右上下の揺れ用タイマー
    float weaveTimer_ = 0.0f;

    // 被弾リアクション（即死しなかった時に一瞬白く光らせる）
    float hitFlashTimer_ = 0.0f;
    Vector4 colorBeforeFlash_{1.0f, 1.0f, 1.0f, 1.0f};
};
