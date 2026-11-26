#pragma once
#include "BossAttackCue.h"
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
    // 誘導弾の玉を count 個、ボスの周りに出す(攻撃の予告の始まりに呼ぶ)。appearSec: 出現の秒数(予告の長さ)。chargeSec: 溜めの秒数
    virtual void SummonOrbs(int count, float appearSec, float chargeSec) = 0;
    // 待っている誘導弾の玉(まだ発射を命じていないもの)を、消し始める。攻撃が発射の前に中断されたとき(シールドの発動など)に、
    // 玉が残らないようにするために使う。発射を命じた玉(飛行中を含む)は、そのまま飛び続ける。何もなければ、何もしない
    virtual void DismissOrbs() = 0;
    // 待っている玉を、1 つずつ(intervalSec の間隔で)、自機へ向けて発射する。flightSec: 発射から自機へ着くまでの時間(秒)。
    // 自機が見つからないときは、待っている玉を消す
    virtual void LaunchOrbs(float flightSec, float intervalSec) = 0;
    // 攻撃の段が変わったことを伝える(体は、その段のアニメーションと演出に切り替える)。攻撃が終わるときは None を渡す。
    // spec: いまの攻撃の定義(演出が、段の長さと種類を読む)
    virtual void SetAttackStage(BossAttackStage stage, const BossAttackCue& cue) = 0;
    // いまのフェーズ(HP の残りで決まる)
    virtual BossPhase GetPhase() const = 0;
    // ボスをよろめかせる(被弾のよろめきのアニメーションを再生する)。シールドが砕けたときなどに使う
    virtual void Stagger() = 0;
    // 全画面の爆発のダメージを、自機に与える。自機が回避の無敵の間は、与えない
    virtual void ApplyBlastDamage(int damage) = 0;
    // シールドの今の状態を、外へ見せる(演出が読む)。シールドが終わるときは、既定の値(stage = None)を渡す
    virtual void SetShieldView(const BossShieldView& view) = 0;
    // シールドを張り始めたことを伝える(展開の段階に入ったとき)。ボスの HP を、nextPhase の境目の値へ戻す
    // (一度に境目を大きく越えて減っていた分を、無かったことにする。シールドを壊すと、次のフェーズを境目の HP から始める)
    virtual void BeginShield(BossPhase nextPhase) = 0;
    // シールドが終わったことを伝える。フェーズを nextPhase へ進める(次の境目を切るまで、境目の通知は来ない)
    virtual void FinishShield(BossPhase nextPhase) = 0;
    // 次の状態を予約する。切り替えは、いま実行中の状態の Update が終わったあとに行われる
    // (Update の途中で、実行中の状態自身が消えてしまわないようにするため)
    virtual void ChangeState(std::unique_ptr<IBossState> next) = 0;
};
