#pragma once
#include "BossAttackPlan.h"
#include "IBossAttackPattern.h"
#include <memory>

// 攻撃の表の 1 行(BossAttackSpec。種類は数値の BossAttackKind)から、攻撃パターンのクラスを作る(Factory)。
//
// 分けた理由: 表はデータとして、種類を数値のまま持つ(並べて読みやすく、調整は数値の編集で済む)。
// 数値をクラスへ変換する分岐は、ここの 1 か所だけにし、状態や演出に種類の分岐を残さない。
namespace BossAttackPatternFactory {

// spec の種類に合うパターンを作る。種類が分からないときは、ログを出して単発弾にする(攻撃が止まらないように)
std::unique_ptr<IBossAttackPattern> Create(const BossAttackSpec& spec);

} // namespace BossAttackPatternFactory
