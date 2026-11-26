#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "BossActions.h"
#include "BossAttackEffects.h"
#include "BossModelSpec.h"
#include "BossOrbGroup.h"
#include "BossPhase.h"
#include "BossShieldEffects.h"
#include "BossState.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {
class ISpawner;
class IScreenView;
class ISceneQuery;
class ModelComponent;
}

// ボスの位置の決まり。出現の最初の 1 フレーム(GameScene)と、毎フレームの移動(BossComponent)が、同じ値を使う
namespace BossPlacement {
// カメラ(=レール上の位置)の前方に留まる距離。基準はカメラで、自機の動きには影響されない。
// 自機はカメラの前 22 にいるので、自機からボスまでは、この値 - 22(46 なら 24)。少し離して、攻撃の演出が画面に収まるようにしている
constexpr float kHoverDistanceFromCamera = 46.0f;
} // namespace BossPlacement

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
    // 攻撃の演出(口の前の輪)を描く。雲の合成のあとに呼ばれる(雲に隠れない)
    void DrawAfterClouds() override;

    std::string GetTypeName() const override { return "BossComponent"; }

    bool IsActive() const { return isActive_; }
    void OnCollision(AbsoluteEngine::GameObject* other) override;
    // hitPosition: 被弾した位置(ワールド)。シールドが受け止めたときに、ドームの波紋を出す位置に使う。nullptr なら、波紋は出ない
    void TakeDamage(int damage, const Vector3* hitPosition = nullptr);

    float GetCollisionRadius() const { return radius_; }
    // 当たり判定の半径を、ボスの種類の定義(BossModelSpec)に合わせる(生成するシーンが呼ぶ)
    void SetCollisionRadius(float radius) { radius_ = radius; }
    // 攻撃の段ごとのアニメーションのクリップを、ボスのモデルの定義(BossModelSpec)に合わせる(生成するシーンが呼ぶ)
    void SetAttackClips(const BossAttackClips& clips) { attackClips_ = clips; }

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    BossPhase GetPhase() const override { return phase_; }
    // シールドの今の状態(演出が読む)。シールドの最中でなければ、stage = None
    const BossShieldView& GetShieldView() const { return shieldView_; }
    // 次のフェーズに入る HP(この値以下で、シールドが始まる)。最後のフェーズなら 0。開発用ショートカット(HP を境目まで減らす)が使う
    int GetNextPhaseBoundaryHp() const;

private:
    // --- IBossActions(状態クラスからの依頼を受ける窓口) ---
    void FireAimedShot(float speed) override;
    void FireSpreadShot(int count, float spreadAngleDeg, float speed) override;
    void SummonOrbs(int count, float appearSec, float chargeSec) override;
    void DismissOrbs() override { orbGroup_.DismissHeld(); }
    void LaunchOrbs(float flightSec, float intervalSec) override;
    void SetAttackStage(BossAttackStage stage, const BossAttackCue& cue) override {
        attackStage_ = stage;
        attackStageChanged_ = (stage != BossAttackStage::None);
        attackEffects_.OnStageChanged(stage, cue);
    }
    void ChangeState(std::unique_ptr<IBossState> next) override { pendingState_ = std::move(next); }
    void Stagger() override { hitRequested_ = true; }
    void ApplyBlastDamage(int damage) override;
    void SetShieldView(const BossShieldView& view) override { shieldView_ = view; }
    void FinishShield(BossPhase nextPhase) override {
        phase_ = nextPhase;
        shieldActive_ = false;
    }

    AbsoluteEngine::ISpawner& spawner_; // 弾を出す窓口
    AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置、画面内か)を知る窓口
    AbsoluteEngine::ISceneQuery& query_; // 自機などを探すために、シーンのオブジェクト一覧を調べる窓口

    // HP から、入るべきフェーズを求める。いまのフェーズより進むときは、すぐには進めず、シールドの状態を始める
    void UpdatePhase();
    // 次のフェーズの境目で、シールドを始める。ボスの HP を、境目の値へ戻して(一度に境目を大きく越えた分を、無かったことにする)、
    // シールドの状態を予約する。nextPhase: シールドが終わったときに進めるフェーズ
    void StartShield_(BossPhase nextPhase);
    // 予約された状態があれば、今の状態を Exit して切り替え、Enter を呼ぶ。最初の 1 回は、待機の状態を作る
    void ApplyPendingState();
    // カメラ(=レール)の前方に留まりつつ左右上下にゆっくり揺れる。自機の位置には影響されない。
    // 向きは、カメラ(=自機のいる側)へ正面を向ける
    void UpdateMovement(float deltaTime);
    // モデルのアニメーションを切り替える(待機 / 攻撃の予告・溜め・発射 / 被弾)。優先度は、被弾 > 攻撃 > 待機
    void UpdateAnimation();
    // いまの攻撃の段のクリップを再生する。切り替えたら true。その段のクリップが無い(nullptr)ときは、何もせず false
    bool PlayAttackStageClip(AbsoluteEngine::ModelComponent& model);
    // シールドのドームがある間は、当たり判定をドームの外側まで広げ、終わったら元の半径へ戻す
    void UpdateColliderForShield_();
    // 弾を出す位置(口の前。muzzle ボーンが無いモデルは、ボスの位置)
    Vector3 GetBulletOrigin_() const;
    // シーンから自機のGameObjectを探す（Fire*で使用）
    std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer() const;

    bool isActive_ = true;
    float radius_ = 5.0f;  // ボスなので当たり判定を大きめに
    int hp_ = 40;          // ボスのHP（3フェーズ分の耐久を持たせる）
    int maxHp_ = 40;

    BossPhase phase_ = BossPhase::Phase1;
    float baseColliderRadius_ = -1.0f; // 当たり判定の元の半径(最初の UpdateColliderForShield_ で覚える。負なら、まだ)
    bool shieldActive_ = false;  // シールドの最中か(始めてから、FinishShield まで)。この間は、フェーズの判定を止める
    BossShieldView shieldView_;  // シールドの今の状態(状態クラスが更新する)

    // 今の状態と、次に切り替える状態(切り替えは、状態の Update が終わったあとに行う)
    std::unique_ptr<IBossState> state_;
    std::unique_ptr<IBossState> pendingState_;

    // 左右上下の揺れ用タイマー
    float weaveTimer_ = 0.0f;

    // 今再生しているアニメーションの種類(None は、まだ何も再生していない状態)
    enum class AnimState { None, Hover, Attack, Hit };
    AnimState animState_ = AnimState::None;
    bool hitRequested_ = false;      // 被弾した(次の UpdateAnimation で、よろめきを再生する)
    // 攻撃の、いまの段と、段が変わった印(次の UpdateAnimation で、その段のアニメーションへ切り替える)
    BossAttackStage attackStage_ = BossAttackStage::None;
    bool attackStageChanged_ = false;
    // 攻撃の段ごとのアニメーションのクリップ(ボスのモデルごとに違う。シーンが、生成時に合わせる)
    BossAttackClips attackClips_;
    // 攻撃の演出(口の前の輪)
    BossAttackEffects attackEffects_;
    // シールドの見た目(三角形の格子のドーム)
    BossShieldEffects shieldEffects_;
    // 誘導弾の玉の集まり(ボスの周りに出す。ボスの動きに合わせて動かす)
    BossOrbGroup orbGroup_;
};
