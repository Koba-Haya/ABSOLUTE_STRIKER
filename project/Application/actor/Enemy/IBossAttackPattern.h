#pragma once
#include "BossAttackCue.h"

class IBossActions;

// ボスの攻撃パターンの基底クラス(Strategy)。単発弾、扇状弾、誘導弾など、攻撃の種類ごとの「振る舞い」を持つ。
//
// 作った理由: 以前は、攻撃の状態(BossAttackState)と演出(BossAttackEffects)が、攻撃の種類(BossAttackKind)を
// switch / if で調べて、撃ち方・予告での準備・輪の見せ方を決めていた(3 か所に分岐が散らばっていた)。
// 種類ごとの違いを、このクラスの派生へ集めると、状態と演出は、基底の参照で呼ぶだけになる。
// 攻撃を増やすときは、派生クラスを 1 つ足し、BossAttackPatternFactory に 1 行足す。状態と演出は変えない。
//
// 攻撃の流れ(予告 -> 溜め -> 発射 -> 隙)と秒数は、全ての攻撃で共通なので、状態(BossAttackState)が持つ。
// パターンは、流れの区切りで呼ばれる「その攻撃ならではの処理」だけを持つ。
class IBossAttackPattern {
public:
    virtual ~IBossAttackPattern() = default;

    // 予告が始まるとき(攻撃の準備。既定では何もしない。誘導弾は、ここで玉を出す)
    virtual void OnTelegraphStart(IBossActions& actions) const { (void)actions; }

    // 溜めが終わった瞬間に、1 回だけ撃つ
    virtual void Fire(IBossActions& actions) const = 0;

    // 攻撃が終わる、または中断されるとき(後片付け。既定では何もしない。誘導弾は、待っている玉を消す)
    virtual void OnEnd(IBossActions& actions) const { (void)actions; }

    // 口の前の輪の見せ方(既定は標準の輪)
    virtual BossMuzzleRingStyle GetMuzzleRingStyle() const { return BossMuzzleRingStyle{}; }
};
