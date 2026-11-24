#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "BossActions.h"
#include "BossPhase.h"
#include "BossState.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {
class ISpawner;
class IScreenView;
class ISceneQuery;
}

// ボス本体のコンポーネント。HP、移動、フェーズの判定、アニメーションの切り替えを受け持つ。
// 攻撃の流れ(待機 -> 予告 -> 溜め -> 発射 -> 隙)は、状態クラス(BossIdleState / BossAttackState)が持つ。
// このクラスは、今の状態を 1 つ持って毎フレーム Update を呼び、状態からの依頼(弾を撃つ、アニメを変える、状態を変える)を
// 窓口(IBossActions)として受ける。
class BossComponent : public AbsoluteEngine::IComponent, private IBossActions {
public:
    // spawner: 弾を出す窓口。screen: カメラの位置と向き(ボスの位置の基準)を知る窓口。query: 自機を探す窓口。
    // いずれも所有しない。このコンポーネントより長く生きること
    BossComponent(AbsoluteEngine::ISpawner& spawner, AbsoluteEngine::IScreenView& screen, AbsoluteEngine::ISceneQuery& query)
        : spawner_(spawner), screen_(screen), query_(query) {}
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
    BossPhase GetPhase() const override { return phase_; }

private:
    // --- IBossActions(状態クラスからの依頼を受ける窓口) ---
    void FireAimedShot(float speed) override;
    void FireSpreadShot(int count, float spreadAngleDeg, float speed) override;
    void PlayAttackAnimation() override { attackRequested_ = true; }
    void ChangeState(std::unique_ptr<IBossState> next) override { pendingState_ = std::move(next); }

    AbsoluteEngine::ISpawner& spawner_; // 弾を出す窓口
    AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置、画面内か)を知る窓口
    AbsoluteEngine::ISceneQuery& query_; // 自機などを探すために、シーンのオブジェクト一覧を調べる窓口

    void UpdatePhase();
    // 予約された状態があれば、今の状態を Exit して切り替え、Enter を呼ぶ。最初の 1 回は、待機の状態を作る
    void ApplyPendingState();
    // カメラ(=レール)の前方に留まりつつ左右上下にゆっくり揺れる。自機の位置には影響されない。
    // 向きは、カメラ(=自機のいる側)へ正面を向ける
    void UpdateMovement(float deltaTime);
    // モデルのアニメーションを切り替える(待機 / 攻撃の予告 / 被弾)。優先度は、被弾 > 攻撃の予告 > 待機
    void UpdateAnimation();
    // シーンから自機のGameObjectを探す（Fire*で使用）
    std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer() const;

    bool isActive_ = true;
    float radius_ = 5.0f;  // ボスなので当たり判定を大きめに
    int hp_ = 40;          // ボスのHP（3フェーズ分の耐久を持たせる）
    int maxHp_ = 40;

    BossPhase phase_ = BossPhase::Phase1;

    // 今の状態と、次に切り替える状態(切り替えは、状態の Update が終わったあとに行う)
    std::unique_ptr<IBossState> state_;
    std::unique_ptr<IBossState> pendingState_;

    // 左右上下の揺れ用タイマー
    float weaveTimer_ = 0.0f;

    // 今再生しているアニメーションの種類(None は、まだ何も再生していない状態)
    enum class AnimState { None, Hover, Attack, Hit };
    AnimState animState_ = AnimState::None;
    bool hitRequested_ = false;      // 被弾した(次の UpdateAnimation で、よろめきを再生する)
    bool attackRequested_ = false;   // 攻撃の予告が始まった(次の UpdateAnimation で、攻撃のアニメーションを再生する)
};
