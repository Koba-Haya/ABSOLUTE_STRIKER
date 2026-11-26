#pragma once
#include "BossAttackPlan.h"
#include "BossPhase.h"
#include "BossShieldPlan.h"
#include "BossState.h"

// ボスの状態: シールド。HP が、次のフェーズの境目を切ったときに、攻撃の状態や待機の状態に割り込んで始まる。
//
// 役目: 展開、維持(耐久と制限時間)、成功(砕ける)、失敗(時間切れの爆発)の流れを持つ。終わったら、フェーズを進めて、待機の状態へ戻る。
// 攻撃の演出との関係: 展開は予告、維持は溜め、失敗の爆発は発射として、攻撃の段(IBossActions::SetAttackStage)を借りる。
//   体のアニメーションと、口の前の輪・粒の演出が、そのまま使える(維持の間は、輪が口へ縮んで、粒が吸い込まれ続ける)。
// ダメージ: 維持の間は、受けたダメージを耐久から引く(AbsorbDamage)。それ以外の段は、ダメージを無かったことにする。
//   どの段でも、ボスの HP は減らない(ボスは、シールドが終わるまで倒せない)。
class BossShieldState : public IBossState {
public:
    // nextPhase: シールドが終わったときに、進めるフェーズ
    explicit BossShieldState(BossPhase nextPhase);

    void Enter(IBossActions& actions) override;
    void Update(IBossActions& actions, float deltaTime) override;
    bool AbsorbDamage(IBossActions& actions, int damage) override;

private:
    enum class Stage { Deploy, Hold, Break, Burst };

    // 段を切り替える(段の始まりの処理を行う)
    void EnterStage_(IBossActions& actions, Stage stage);
    // 外へ見せる状態を、毎フレーム更新する
    void PublishView_(IBossActions& actions) const;
    // 現在の段の長さ(秒)
    float GetStageSeconds_() const;
    // シールドを終える: フェーズを進めて、待機の状態へ戻る
    void Finish_(IBossActions& actions);

    BossPhase nextPhase_;
    Stage stage_ = Stage::Deploy;
    float timer_ = 0.0f;  // いまの段に入ってからの時間
    int shieldHp_ = BossShieldDefaults::kShieldHp;
    BossAttackSpec effectSpec_{}; // 攻撃の演出に渡す定義(段の長さだけを使う)
};
