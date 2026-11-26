#pragma once
#include <string>

namespace AbsoluteEngine {
class GameObject;
}

// 自機のロックオンと、ロックオン弾(ホーミング弾)が狙える対象の判定を、1 か所にまとめたもの。
//
// 作った理由: 「狙える対象か」「まだ生きているか」の判定が、ロックオンの検索(LockOnSystem)、ロックを外す判定(LockOnSystem)、
// ロック弾の追尾の生死確認(HomingBulletComponent)、ロック弾の当たり判定(HomingBulletComponent)、通常弾の当たり判定(BulletComponent)に、
// 敵とボスだけを前提に、それぞれ書かれていた。対象が増える(ボスの誘導弾の玉)たびに、全部を直し忘れないよう、ここに集める。
// 対象の種類: 通常の敵(Enemy)、ボス(Boss)、ボスの誘導弾の玉(EnemyOrb)。
namespace LockOnTarget {

// タグが、自機の弾が当たる対象(狙える対象)のものか
bool IsTargetTag(const std::string& tag);

// 対象が、まだ生きていて、狙えるか。死んだ敵、HP が 0 のボス、終わった玉(撃ち落とされた、消えた、まだ現れきっていない玉も)は、狙えない。
// 生死を表す部品(EnemyComponent、BossComponent、BossOrbComponent)を持たない対象は、生きているものとして扱う(true)
bool IsAlive(AbsoluteEngine::GameObject& target);

} // namespace LockOnTarget
