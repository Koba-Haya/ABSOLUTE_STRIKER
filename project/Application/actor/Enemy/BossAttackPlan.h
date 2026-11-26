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
    // 誘導弾: 予告でボスの周りに玉が現れ、溜めの間は固定で待ち、発射で 1 つずつ自機へ飛ぶ(自機は、玉を撃ち落とせる)。
    // 玉の数は count。発射の間隔は launchIntervalSec、飛行の時間は flightSec。speed と spreadAngleDeg は使わない。
    // 予告と溜めの間が、玉を撃ち落とす猶予になるので、ほかの攻撃より長い
    HomingOrbs,
};

// 1 回の攻撃の定義
struct BossAttackSpec {
    BossAttackKind kind;
    float idleSec;      // この攻撃の前の待機(秒)
    float telegraphSec; // 予告(構えへ移る。秒)
    float chargeSec;    // 溜め(構えたまま力を溜める。秒)
    float recoverSec;   // 発射のあとの隙(秒)
    float speed;        // 弾速
    int count;          // 弾の数(Spread のとき。Aimed は 1 発なので使わない)
    float spreadAngleDeg; // 弾と弾の間の角度(度。Spread のとき)
    // 誘導弾(HomingOrbs)のとき。ほかの攻撃では使わない(表の行では、省略してよい)
    float launchIntervalSec = 0.0f; // 玉を 1 つずつ発射する間隔(秒)
    float flightSec = 0.0f;         // 発射から自機へ着くまでの時間(秒。速くしすぎない)
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
// 予告と溜めの秒数。フェーズが上がるほど短くする(弾幕の緊張感を保つ)。攻撃の周期は、従来の値に合わせない(予告と溜めを見せるため)。
// 予告のクリップ(AttackTelegraph)は 0.6 秒。これより短い予告は、クリップの途中で溜めへ切り替わる(短い溜めのとき、構えが浅くなる)
constexpr float kTelegraphSecPhase1 = 0.6f;
constexpr float kChargeSecPhase1 = 0.6f;
constexpr float kTelegraphSecPhase2 = 0.5f;
constexpr float kChargeSecPhase2 = 0.5f;
constexpr float kTelegraphSecPhase3 = 0.4f;
constexpr float kChargeSecPhase3 = 0.4f;
// 発射のあとの隙。発射のクリップ(AttackFire)の長さ(0.5 秒)に合わせる(突き出して戻るまでを、隙の間に見せる)
constexpr float kRecoverSec = 0.5f;
// 扇状弾の数
constexpr int kSpreadCountPhase2 = 5;
constexpr int kSpreadCountPhase3 = 8;
// 誘導弾(玉)の数
constexpr int kOrbCountPhase2 = 4;
constexpr int kOrbCountPhase3 = 6;
// 誘導弾の予告(玉が現れる)と溜め(玉が固定で待つ。撃ち落とす猶予)。ほかの攻撃より長い。フェーズが上がるほど、少し短くする
constexpr float kOrbTelegraphSecPhase2 = 0.9f;
constexpr float kOrbChargeSecPhase2 = 1.2f;
constexpr float kOrbTelegraphSecPhase3 = 0.8f;
constexpr float kOrbChargeSecPhase3 = 1.0f;
// 誘導弾の、玉を 1 つずつ発射する間隔と、飛行の時間(速くしすぎない。撃ち落とす猶予になる)
constexpr float kOrbLaunchIntervalSec = 0.25f;
constexpr float kOrbFlightSecPhase2 = 2.2f;
constexpr float kOrbFlightSecPhase3 = 2.0f;
} // namespace BossAttackPlanDefaults

namespace BossAttackPlanTable {
using namespace BossAttackPlanDefaults;

// 1 回の攻撃の周期 = idleSec + 予告 + 溜め + recoverSec
inline constexpr BossAttackSpec kPhase1[] = {
    { BossAttackKind::Aimed, 0.5f, kTelegraphSecPhase1, kChargeSecPhase1, kRecoverSec, kAimedSpeed, 1, 0.0f },
};
inline constexpr BossAttackSpec kPhase2[] = {
    { BossAttackKind::Aimed,  0.4f, kTelegraphSecPhase2, kChargeSecPhase2, kRecoverSec, kAimedSpeed,  1, 0.0f },
    // 扇状弾は、プレイ会で邪魔になるため、一時的に表から外している(攻撃の仕組みと数値は残してある。戻すときは、この行を有効にする)
    // { BossAttackKind::Spread, 0.6f, kTelegraphSecPhase2, kChargeSecPhase2, kRecoverSec, kSpreadSpeed, kSpreadCountPhase2, kSpreadAngleDeg },
    { BossAttackKind::HomingOrbs, 0.6f, kOrbTelegraphSecPhase2, kOrbChargeSecPhase2, kRecoverSec, 0.0f, kOrbCountPhase2, 0.0f, kOrbLaunchIntervalSec, kOrbFlightSecPhase2 },
};
inline constexpr BossAttackSpec kPhase3[] = {
    { BossAttackKind::Aimed,  0.1f, kTelegraphSecPhase3, kChargeSecPhase3, kRecoverSec, kAimedSpeed,  1, 0.0f },
    { BossAttackKind::Aimed,  0.1f, kTelegraphSecPhase3, kChargeSecPhase3, kRecoverSec, kAimedSpeed,  1, 0.0f },
    // 扇状弾は、一時的に表から外している(上の Phase2 と同じ理由)
    // { BossAttackKind::Spread, 0.3f, kTelegraphSecPhase3, kChargeSecPhase3, kRecoverSec, kSpreadSpeed, kSpreadCountPhase3, kSpreadAngleDeg },
    { BossAttackKind::HomingOrbs, 0.4f, kOrbTelegraphSecPhase3, kOrbChargeSecPhase3, kRecoverSec, 0.0f, kOrbCountPhase3, 0.0f, kOrbLaunchIntervalSec, kOrbFlightSecPhase3 },
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
