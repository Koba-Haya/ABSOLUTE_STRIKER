#pragma once
#include "BossActions.h"
#include "BossAttackCue.h"
#include "BossPhase.h"
#include "BossShieldPlan.h"
#include "BossState.h"

// ボスのシールドの段階(展開 / 維持 / 成功 / 失敗)に共通の中間クラス。
//
// シールドの段階に共通すること:
//   - 受けたダメージは、すべて受け止める(ボスの HP は減らない。シールドが終わるまで、ボスは倒せない)。耐久を減らすのは維持の段階だけ
//   - フェーズの境目の通知は無視する(すでにシールドを張っている)
//   - 決まった時間が過ぎたら次へ進む(Template Method: Update が時間を数え、時間切れのときの処理だけを派生が決める)
//   - 外へ見せるシールドの状態(BossShieldView)を、毎フレーム出す
// 攻撃の演出との関係: 展開は予告、維持は溜め、失敗の爆発は発射として、攻撃の段(IBossActions::SetAttackStage)を借りる。
//   体のアニメーションと、口の前の輪・粒の演出が、そのまま使える。
// 以前は 1 つのクラス(BossShieldState)の中で、段を enum と switch で切り替えていた。段ごとに、入ったときの処理・ダメージの受け方・
// 次の段が違うので、段ごとの状態クラスに分けた。
class BossShieldStageState : public IBossState {
public:
    // nextPhase: シールドが終わったときに進めるフェーズ。shieldHp: この段の始まりの、シールドの耐久
    BossShieldStageState(BossPhase nextPhase, int shieldHp);

    // 時間を数え、この段の長さが過ぎたら OnTimeUp_ を呼ぶ。過ぎていなければ、外へ見せる状態を出す
    void Update(IBossActions& actions, float deltaTime) final;
    // ダメージをすべて受け止める(OnShieldHit_ で、段ごとの受け方を決める)
    bool AbsorbDamage(IBossActions& actions, int damage) final;
    // すでにシールドを張っているので、境目の通知は無視する
    void OnPhaseBoundaryCrossed(IBossActions& actions, BossPhase nextPhase) final;

protected:
    // 段に入ったときの共通の処理: ログを残し、攻撃の段(演出)を切り替え、外へ見せる状態を出す
    void EnterStage_(IBossActions& actions, BossAttackStage attackStage);
    // 外へ見せるシールドの状態を出す
    void PublishView_(IBossActions& actions) const;
    // シールドを終える: 攻撃の演出と、外へ見せる状態を片づけ、フェーズを進めて、待機の状態へ戻る
    void Finish_(IBossActions& actions);

    BossPhase GetNextPhase() const { return nextPhase_; }
    int GetShieldHp() const { return shieldHp_; }
    void SetShieldHp(int shieldHp) { shieldHp_ = shieldHp; }
    // この段に入ってからの時間(秒)
    float GetTimer() const { return timer_; }
    // この段の中の進み具合(0 〜 1)
    float GetStageProgress() const;

private:
    // この段の長さ(秒)
    virtual float Seconds_() const = 0;
    // 外へ見せる、この段の名札
    virtual BossShieldStage ViewStage_() const = 0;
    // この段の長さが過ぎたときの処理(次の段へ進む、またはシールドを終える)
    virtual void OnTimeUp_(IBossActions& actions) = 0;
    // ダメージを受けたときの、この段の処理。既定は何もしない(受け止めるだけ)
    virtual void OnShieldHit_(IBossActions& actions, int damage) { (void)actions; (void)damage; }
    // 外へ見せる状態の、残り時間の割合と警告を決める。既定は、残り時間の表示なし(0)、警告なし
    virtual void FillTime_(BossShieldView& view) const { (void)view; }

    BossPhase nextPhase_;
    int shieldHp_;
    float timer_ = 0.0f;        // この段に入ってからの時間
    BossAttackCue effectCue_{}; // 攻撃の演出に渡す情報(展開を予告、維持を溜めとして見せる長さ。輪は標準の見せ方)
};
