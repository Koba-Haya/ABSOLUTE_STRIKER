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
    // 当たり判定の半径を、ボスの種類の定義(BossModelSpec)に合わせる(生成するシーンが呼ぶ)
    void SetCollisionRadius(float radius) { radius_ = radius; }

    // 内側(コア)のモデルを持つオブジェクト(任意)。コアは外殻とは別のオブジェクトなので(外殻だけをディゾルブで消すため)、
    // このコンポーネントが、位置・向き・大きさと、アニメーションを、外殻に合わせ続ける
    void SetCoreObject(std::shared_ptr<AbsoluteEngine::GameObject> core) { coreObject_ = std::move(core); }
    const std::shared_ptr<AbsoluteEngine::GameObject>& GetCoreObject() const { return coreObject_; }

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    BossPhase GetPhase() const { return phase_; }

private:
    void UpdatePhase();
    void UpdateAttack(float deltaTime);
    // カメラ(=レール)の前方に留まりつつ左右上下にゆっくり揺れる。自機の位置には影響されない。
    // 向きは、カメラ(=自機のいる側)へ正面を向ける
    void UpdateMovement(float deltaTime);
    // モデルのアニメーションを切り替える(待機 / 攻撃の予告 / 被弾)。優先度は、被弾 > 攻撃の予告 > 待機
    void UpdateAnimation();
    // 外殻と、コア(あれば)の両方で、同じアニメーションを再生する(同じ時刻に同じポーズで動かすため)
    void PlayAnimationOnAll(const char* name, bool loop, float blendSeconds, bool restart);
    // コアのオブジェクトの Transform を、外殻(このボス)に合わせる
    void SyncCoreTransform();

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

    // 左右上下の揺れ用タイマー
    float weaveTimer_ = 0.0f;

    // 内側(コア)のモデルのオブジェクト(無いボスでは null)
    std::shared_ptr<AbsoluteEngine::GameObject> coreObject_;

    // 今再生しているアニメーションの種類(None は、まだ何も再生していない状態)
    enum class AnimState { None, Hover, Attack, Hit };
    AnimState animState_ = AnimState::None;
    bool hitRequested_ = false;      // 被弾した(次の UpdateAnimation で、よろめきを再生する)
    bool attackRequested_ = false;   // 単発弾の発射が近い(次の UpdateAnimation で、攻撃の予告を再生する)
    bool attackAnnounced_ = false;   // 今回の発射の予告を、すでに出したか(1 回の発射につき 1 回だけ出す)
};
