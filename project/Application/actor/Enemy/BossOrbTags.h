#pragma once

// ボスの誘導弾の「玉」に関わる GameObject のタグ(GameObject::GetTag の値)。
//
// 分けた理由: 玉を出す側(BossOrbGroup)、玉の尾を残す側(BossOrbVisualComponent)、ロックオンの対象を判別する側(LockOnTarget)、
// 演出の間に残った弾を消す側(GameScene)が、同じ文字列を使う。文字列を各所に書くと、1 か所を直し忘れて、判別が食い違うので、ここに集める。
namespace BossOrbTags {
// 玉本体。通常の敵(Enemy)と分ける: Enemy にすると、撃破率などの集計が、玉を敵として数えてしまうため
constexpr const char* kOrb = "EnemyOrb";
// 玉が消えたあとに残る尾(EnergyRibbonFadeComponent を持つ GameObject)。演出の間に、玉と一緒に消す対象
constexpr const char* kTrail = "EnemyOrbTrail";
} // namespace BossOrbTags
