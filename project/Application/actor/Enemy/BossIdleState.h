#pragma once
#include "BossState.h"
#include <cstddef>

// ボスの状態: 待機(次の攻撃までの間)。
//
// 役目: 攻撃の表(BossAttackPlan.h)から、次の攻撃を選び、その攻撃の前の待機時間(idleSec)が過ぎたら、攻撃の状態へ進む。
// ボスの移動(左右上下の揺れ)は、状態によらず BossComponent が毎フレーム行う。ここで待つのは、攻撃だけ。
class BossIdleState : public IBossState {
public:
    // attackIndex: 次に行う攻撃が、フェーズの表の何番目か(表の長さを超えたら、先頭へ戻る)
    explicit BossIdleState(std::size_t attackIndex) : attackIndex_(attackIndex) {}

    void Enter(IBossActions& actions) override;
    void Update(IBossActions& actions, float deltaTime) override;

private:
    std::size_t attackIndex_;
    float waitSec_ = 0.0f; // 待つ秒数(Enter で、選んだ攻撃の idleSec に決める)
    float timer_ = 0.0f;   // 待った時間
};
