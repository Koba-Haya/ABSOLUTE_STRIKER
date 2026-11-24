#pragma once
#include "BossAttackPlan.h"
#include "BossState.h"
#include <cstddef>

// ボスの状態: 攻撃 1 回。
//
// 流れ: 予告(Telegraph) -> 溜め(Charge) -> 発射 -> 隙(Recover) -> 待機の状態へ戻る。
// 各段の秒数と、攻撃の種類(単発、扇状)は、攻撃の定義(BossAttackSpec)で決まる。
// 発射は、溜めが終わった瞬間に 1 回だけ行う。段が変わるたびに、IBossActions::SetAttackStage で体へ伝える
// (体は、段ごとのアニメーションに切り替える。今後、演出(粒、光)も、ここへ結びつける)。
// 段(Telegraph / Charge / Recover)は、秒数の表だけで進むので、状態クラスには分けず、このクラスの中の列挙体で持つ。
class BossAttackState : public IBossState {
public:
    // spec: 行う攻撃の定義(コピーして持つ)。attackIndex: この攻撃が、フェーズの表の何番目か(次の攻撃は、その次の番号)
    BossAttackState(const BossAttackSpec& spec, std::size_t attackIndex) : spec_(spec), attackIndex_(attackIndex) {}

    void Enter(IBossActions& actions) override;
    void Update(IBossActions& actions, float deltaTime) override;
    void Exit(IBossActions& actions) override;

private:
    enum class Stage { Telegraph, Charge, Recover };

    // いまの段の秒数
    float GetStageSec() const;
    // 攻撃の種類に合わせて、弾を撃つ
    void Fire(IBossActions& actions) const;

    BossAttackSpec spec_;
    std::size_t attackIndex_;
    Stage stage_ = Stage::Telegraph;
    float timer_ = 0.0f; // いまの段に入ってからの時間
};
