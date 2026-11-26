#pragma once
#include "BossAttackCue.h"
#include "BossAttackPlan.h"
#include "BossState.h"
#include "IBossAttackPattern.h"
#include <cstddef>
#include <memory>

// ボスの状態: 攻撃 1 回。
//
// 流れ: 予告(Telegraph) -> 溜め(Charge) -> 発射 -> 隙(Recover) -> 待機の状態へ戻る。
// 各段の秒数は、攻撃の定義(BossAttackSpec)で決まる。攻撃の種類ごとの振る舞い(撃ち方、予告での準備、後片付け、輪の見せ方)は、
// 攻撃パターン(IBossAttackPattern の派生)に任せる。この状態は、攻撃の種類を知らない(流れの区切りで、パターンを呼ぶだけ)。
// 発射は、溜めが終わった瞬間に 1 回だけ行う。段が変わるたびに、IBossActions::SetAttackStage で体へ伝える
// (体は、段ごとのアニメーションに切り替える。今後、演出(粒、光)も、ここへ結びつける)。
// 段(Telegraph / Charge / Recover)は、秒数の表だけで進むので、状態クラスには分けず、このクラスの中の列挙体で持つ。
class BossAttackState : public IBossState {
public:
    // spec: 行う攻撃の定義(コピーして持つ)。attackIndex: この攻撃が、フェーズの表の何番目か(次の攻撃は、その次の番号)
    BossAttackState(const BossAttackSpec& spec, std::size_t attackIndex);

    void Enter(IBossActions& actions) override;
    void Update(IBossActions& actions, float deltaTime) override;
    void Exit(IBossActions& actions) override;

private:
    enum class Stage { Telegraph, Charge, Recover };

    // いまの段の秒数
    float GetStageSec() const;
    BossAttackSpec spec_;
    std::unique_ptr<IBossAttackPattern> pattern_; // 攻撃の種類ごとの振る舞い(spec_ から作る)
    BossAttackCue cue_;                           // 演出へ渡す情報(段の長さと、輪の見せ方)
    std::size_t attackIndex_;
    Stage stage_ = Stage::Telegraph;
    float timer_ = 0.0f; // いまの段に入ってからの時間
};
