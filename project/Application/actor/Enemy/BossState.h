#pragma once
#include "BossPhase.h"

class IBossActions;

// ボスの「状態」の共通の形(State パターンの、状態の基底クラス)。
//
// 状態クラスは、「今この状態のとき、ボスは何をするか」だけを持つ。BossComponent は、今の状態を 1 つ持ち、毎フレーム Update を呼ぶだけ。
// 次にどの状態へ進むかは、状態クラス自身が決める(IBossActions::ChangeState を呼ぶ)。
// ボスの体(弾を撃つ、アニメを変える)は、窓口(IBossActions)経由で触る。BossComponent を直接知らないので、依存が一方向になる。
//
// 状態の一覧: BossIdleState(次の攻撃までの待機)、BossAttackState(攻撃 1 回)、
// シールドの 4 段階(BossShieldDeployState -> BossShieldHoldState -> BossShieldBreakState / BossShieldBurstState。中間クラス BossShieldStageState)。
class IBossState {
public:
    virtual ~IBossState() = default;

    // この状態に入ったときに 1 回呼ばれる。actions: ボスの体への窓口
    virtual void Enter(IBossActions& actions) { (void)actions; }

    // 毎フレーム呼ばれる。deltaTime: 前のフレームからの経過秒。
    // 状態を変えたいときは actions.ChangeState を呼ぶ(切り替えは、この Update が終わったあとに行われる)
    virtual void Update(IBossActions& actions, float deltaTime) = 0;

    // この状態から出るときに 1 回呼ばれる
    virtual void Exit(IBossActions& actions) { (void)actions; }

    // ボスが攻撃を受けたとき(HP が減る前)に呼ばれる。この状態がダメージを受け止めるなら true を返す(ボスの HP は減らず、よろめきもしない)。
    // 既定は false(受け止めない。ボスの HP が減る)。シールドの状態が、これを上書きする
    virtual bool AbsorbDamage(IBossActions& actions, int damage) { (void)actions; (void)damage; return false; }

    // ボスの HP が、次のフェーズの境目を切ったときに呼ばれる(境目を切っている間、毎フレーム)。境目を見つけるのは BossComponent、
    // それを受けてどうするか(シールドを張るか)は、今の状態が決める。
    // 既定は、すぐにシールドの展開(BossShieldDeployState)へ進む(待機や攻撃を中断する)。シールドの段階は、すでに張っているので無視する。
    // nextPhase: シールドが終わったときに進めるフェーズ
    virtual void OnPhaseBoundaryCrossed(IBossActions& actions, BossPhase nextPhase);
};
