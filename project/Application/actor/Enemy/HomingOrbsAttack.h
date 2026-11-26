#pragma once
#include "IBossAttackPattern.h"

// 攻撃パターン: 誘導弾。予告でボスの周りに玉が現れ、溜めの間は固定で待ち、発射で 1 つずつ自機へ飛ぶ。
// 口から弾は出ないので、口の前の輪は出さない(予告と溜めは、玉そのものが見せる)
class HomingOrbsAttack final : public IBossAttackPattern {
public:
    // 引数の意味は BossAttackSpec と同じ(count: 玉の数, telegraphSec: 玉が現れる秒数, chargeSec: 玉が待つ秒数,
    // flightSec: 発射から自機へ着くまでの秒数, launchIntervalSec: 玉を 1 つずつ発射する間隔)
    HomingOrbsAttack(int count, float telegraphSec, float chargeSec, float flightSec, float launchIntervalSec)
        : count_(count), telegraphSec_(telegraphSec), chargeSec_(chargeSec), flightSec_(flightSec), launchIntervalSec_(launchIntervalSec) {}

    void OnTelegraphStart(IBossActions& actions) const override;
    void Fire(IBossActions& actions) const override;
    void OnEnd(IBossActions& actions) const override;
    BossMuzzleRingStyle GetMuzzleRingStyle() const override;

private:
    int count_;               // 玉の数
    float telegraphSec_;      // 玉が現れる秒数(予告の長さ)
    float chargeSec_;         // 玉が固定で待つ秒数(溜めの長さ)
    float flightSec_;         // 発射から自機へ着くまでの秒数
    float launchIntervalSec_; // 玉を 1 つずつ発射する間隔(秒)
};
