#pragma once
#include "BossPhase.h"
#include <cstddef>

// ボスの攻撃の「表」(データ)。フェーズごとに、どの攻撃をどの順番で、どんな間隔で行うかを決める。
//
// 分けた理由: 攻撃の種類や数値を、状態クラスのコードに埋め込まず、表として 1 か所に集めるため。
// 調整は、ここの数値を変えるだけで済む。フェーズごとに攻撃を足す・並べ替えるのも、表の編集で済む。
//
// 1 回の攻撃の流れ:  待機(idleSec) -> 予告(telegraphSec) -> 溜め(chargeSec) -> 発射 -> 隙(recoverSec) -> 次の攻撃の待機
// 攻撃は 1 つずつ順番に行う(予告が重なって、何が来るか分からなくなるのを避けるため)。
// フェーズの表の最後まで進んだら、先頭へ戻る。

// 攻撃の種類
enum class BossAttackKind {
    Aimed,  // 自機狙いの単発弾
    Spread, // 自機方向を中心にした扇状弾
};

// 1 回の攻撃の定義
struct BossAttackSpec {
    BossAttackKind kind;
    float idleSec;      // この攻撃の前の待機(秒)
    float telegraphSec; // 予告(あごを開き始める。秒)
    float chargeSec;    // 溜め(発射の直前。秒)
    float recoverSec;   // 発射のあとの隙(秒)
    float speed;        // 弾速
    int count;          // 弾の数(Spread のとき。Aimed は 1 発なので使わない)
    float spreadAngleDeg; // 弾と弾の間の角度(度。Spread のとき)
};

// 1 つのフェーズの攻撃の並び
struct BossAttackSequence {
    const BossAttackSpec* specs;
    std::size_t count;
};

namespace BossAttackPlanDefaults {
// 弾速
constexpr float kAimedSpeed = 24.0f;
constexpr float kSpreadSpeed = 16.0f;
// 扇状弾の弾と弾の間の角度(度)
constexpr float kSpreadAngleDeg = 12.0f;
// 予告と溜めの秒数(合計 0.6 秒。従来の「発射の 0.6 秒前に、攻撃の予告を始める」と同じ)
constexpr float kTelegraphSec = 0.3f;
constexpr float kChargeSec = 0.3f;
// 扇状弾の数
constexpr int kSpreadCountPhase2 = 5;
constexpr int kSpreadCountPhase3 = 8;
} // namespace BossAttackPlanDefaults

namespace BossAttackPlanTable {
using namespace BossAttackPlanDefaults;

// idleSec と recoverSec の合計と、予告 + 溜めの 0.6 秒で、1 回の攻撃の周期が決まる。
// 従来の間隔(Phase1: 単発 1.6 秒)に近くなるよう決めている。予告を入れた分、Phase3 の密度は、従来より少し下がる
inline constexpr BossAttackSpec kPhase1[] = {
    { BossAttackKind::Aimed, 0.8f, kTelegraphSec, kChargeSec, 0.2f, kAimedSpeed, 1, 0.0f },
};
inline constexpr BossAttackSpec kPhase2[] = {
    { BossAttackKind::Aimed,  0.5f, kTelegraphSec, kChargeSec, 0.2f, kAimedSpeed,  1, 0.0f },
    { BossAttackKind::Spread, 0.9f, kTelegraphSec, kChargeSec, 0.3f, kSpreadSpeed, kSpreadCountPhase2, kSpreadAngleDeg },
};
inline constexpr BossAttackSpec kPhase3[] = {
    { BossAttackKind::Aimed,  0.1f, kTelegraphSec, kChargeSec, 0.2f, kAimedSpeed,  1, 0.0f },
    { BossAttackKind::Aimed,  0.1f, kTelegraphSec, kChargeSec, 0.2f, kAimedSpeed,  1, 0.0f },
    { BossAttackKind::Spread, 0.3f, kTelegraphSec, kChargeSec, 0.3f, kSpreadSpeed, kSpreadCountPhase3, kSpreadAngleDeg },
};
} // namespace BossAttackPlanTable

// フェーズの攻撃の並びを返す
inline BossAttackSequence GetBossAttackSequence(BossPhase phase) {
    using namespace BossAttackPlanTable;
    switch (phase) {
    case BossPhase::Phase2:
        return { kPhase2, sizeof(kPhase2) / sizeof(kPhase2[0]) };
    case BossPhase::Phase3:
        return { kPhase3, sizeof(kPhase3) / sizeof(kPhase3[0]) };
    case BossPhase::Phase1:
    default:
        return { kPhase1, sizeof(kPhase1) / sizeof(kPhase1[0]) };
    }
}

// フェーズの表から、index 番目の攻撃を取り出す。index が表の長さを超えたら、先頭へ戻る(剰余)
inline const BossAttackSpec& PickBossAttack(BossPhase phase, std::size_t index) {
    const BossAttackSequence seq = GetBossAttackSequence(phase);
    return seq.specs[index % seq.count];
}
