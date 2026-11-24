#pragma once
#include "BossAttackPlan.h"
#include "BossPhase.h"
#include "BossShieldPlan.h"
#include "BossState.h"
#include <memory>

// 状態クラス(IBossState)から見た、ボスの体への窓口。
//
// 分けた理由: 状態クラスが BossComponent を直接触ると、BossComponent(状態を持つ)と状態クラス(BossComponent を使う)が
// お互いを知る循環参照になる。状態クラスには、このインターフェースだけを見せ、BossComponent がこれを実装する。
// 状態が使える操作を、ここに書いた最小限に絞る役目もある。
// 攻撃の、いまの段。攻撃のアニメーションと、演出を、段ごとに切り替えるために、状態から体へ伝える。
// Fire は、発射の瞬間から、発射のアニメーションが終わるまで(攻撃の隙の間)
enum class BossAttackStage {
    None,      // 攻撃の最中ではない
    Telegraph, // 予告: 構えへ移る
    Charge,    // 溜め: 構えたまま力を溜める
    Fire,      // 発射と、そのあとの隙
};

class IBossActions {
public:
    virtual ~IBossActions() = default;

    // 自機へ向かう単発弾を 1 発撃つ。speed: 弾速
    virtual void FireAimedShot(float speed) = 0;
    // 自機方向を中心に、扇状の弾を count 発撃つ。spreadAngleDeg: 弾と弾の間の角度(度)。speed: 弾速
    virtual void FireSpreadShot(int count, float spreadAngleDeg, float speed) = 0;
    // 攻撃の段が変わったことを伝える(体は、その段のアニメーションと演出に切り替える)。攻撃が終わるときは None を渡す。
    // spec: いまの攻撃の定義(演出が、段の長さと種類を読む)
    virtual void SetAttackStage(BossAttackStage stage, const BossAttackSpec& spec) = 0;
    // いまのフェーズ(HP の残りで決まる)
    virtual BossPhase GetPhase() const = 0;
    // ボスをよろめかせる(被弾のよろめきのアニメーションを再生する)。シールドが砕けたときなどに使う
    virtual void Stagger() = 0;
    // 全画面の爆発のダメージを、自機に与える。自機が回避の無敵の間は、与えない
    virtual void ApplyBlastDamage(int damage) = 0;
    // シールドの今の状態を、外へ見せる(演出が読む)。シールドが終わるときは、既定の値(stage = None)を渡す
    virtual void SetShieldView(const BossShieldView& view) = 0;
    // シールドが終わったことを伝える。フェーズを nextPhase へ進めて、次の境目まで、シールドを張らない状態へ戻す
    virtual void FinishShield(BossPhase nextPhase) = 0;
    // 次の状態を予約する。切り替えは、いま実行中の状態の Update が終わったあとに行われる
    // (Update の途中で、実行中の状態自身が消えてしまわないようにするため)
    virtual void ChangeState(std::unique_ptr<IBossState> next) = 0;
};
