#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {
class ISpawner;
class IScreenView;
}

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
    // spawner: 弾を出すときに使う、オブジェクトを出す窓口。screen: カメラの位置と向き(ボスの位置の基準)を知る、画面の窓口。
    // どちらも所有しない。このコンポーネントより長く生きること
    BossComponent(AbsoluteEngine::ISpawner& spawner, AbsoluteEngine::IScreenView& screen) : spawner_(spawner), screen_(screen) {}
    ~BossComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "BossComponent"; }

    bool IsActive() const { return isActive_; }
    void OnCollision(AbsoluteEngine::GameObject* other) override;
    void TakeDamage(int damage);

    float GetCollisionRadius() const { return radius_; }
    // 当たり判定の半径を、ボスの種類の定義(BossModelSpec)に合わせる(生成するシーンが呼ぶ)
    void SetCollisionRadius(float radius) { radius_ = radius; }

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    BossPhase GetPhase() const { return phase_; }

private:
    AbsoluteEngine::ISpawner& spawner_; // 弾を出す窓口
    AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置、画面内か)を知る窓口

    void UpdatePhase();
    void UpdateAttack(float deltaTime);
    // カメラ(=レール)の前方に留まりつつ左右上下にゆっくり揺れる。自機の位置には影響されない。
    // 向きは、カメラ(=自機のいる側)へ正面を向ける
    void UpdateMovement(float deltaTime);
    // モデルのアニメーションを切り替える(待機 / 攻撃の予告 / 被弾)。優先度は、被弾 > 攻撃の予告 > 待機
    void UpdateAnimation();
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

    // 今再生しているアニメーションの種類(None は、まだ何も再生していない状態)
    enum class AnimState { None, Hover, Attack, Hit };
    AnimState animState_ = AnimState::None;
    bool hitRequested_ = false;      // 被弾した(次の UpdateAnimation で、よろめきを再生する)
    bool attackRequested_ = false;   // 単発弾の発射が近い(次の UpdateAnimation で、攻撃の予告を再生する)
    bool attackAnnounced_ = false;   // 今回の発射の予告を、すでに出したか(1 回の発射につき 1 回だけ出す)
};
