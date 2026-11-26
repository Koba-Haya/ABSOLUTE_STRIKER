#pragma once
#include <cstdint>

// ボスの攻撃の演出(口の前の輪と粒。BossAttackEffects)へ渡す、「どう見せるか」の情報。
//
// 作った理由: 以前は、演出が攻撃の定義(BossAttackSpec)を受け取り、攻撃の種類(kind)を if で調べて、
// 「誘導弾なら輪を出さない」「扇状弾なら弧を弾の数にし、輪を大きくする」を決めていた。
// 種類ごとの違いは、攻撃パターンのクラス(IBossAttackPattern の派生)が、この情報として渡す。演出は攻撃の種類を知らない。
// シールドの状態も、攻撃のふりをせずに、この情報を直接作って、演出を借りられる。

namespace BossMuzzleRingDefaults {
// 標準の輪の弧の数(単発弾、シールド)
constexpr uint32_t kStandardArcCount = 4;
// 標準の輪の大きさの倍率
constexpr float kStandardScale = 1.0f;
} // namespace BossMuzzleRingDefaults

// 口の前の輪の見せ方
struct BossMuzzleRingStyle {
    bool visible = true;                                          // 輪を出すか(誘導弾は、ボスの周りの玉が予告を見せるので、出さない)
    uint32_t arcCount = BossMuzzleRingDefaults::kStandardArcCount; // 弧の数
    float scaleMultiplier = BossMuzzleRingDefaults::kStandardScale; // 輪の大きさに掛ける倍率
};

// 攻撃の段(予告、溜め、発射)を演出へ伝えるときの情報
struct BossAttackCue {
    float telegraphSec = 0.0f; // 予告の長さ(秒)
    float chargeSec = 0.0f;    // 溜めの長さ(秒)
    BossMuzzleRingStyle ring;  // 口の前の輪の見せ方
};
