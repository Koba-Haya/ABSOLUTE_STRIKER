// ============================================================
// EnemyShootUtil.h
// 役割: 敵の射撃コンポーネントが共通で使う、2つの判定(画面内か、自機はどこか)。
//
// なぜ別ファイルか: EnemyShootComponent と TurretShootComponent は、同じ判定をそれぞれのファイルに
//       書いている(重複)。新しい射撃コンポーネントで3つ目を作らないために、1責務(射撃の前提条件の
//       判定)として切り出した。既存の2つは、見た目の挙動を変えないため、この作業では触っていない。
// ============================================================
#pragma once
#include "Type/Vector.h"
#include <memory>

class BaseScene;

namespace AbsoluteEngine {
class GameObject;
} // namespace AbsoluteEngine

namespace EnemyShootUtil {

// 世界座標 position が、メインカメラの画面内かを返す。
// ndcMargin は、画面の端(NDC で ±1)からどこまでを「画面内」とみなすか(1.0 ちょうど画面内、1.2 なら端の外 20% まで)。
// カメラの背後にあるとき、カメラが無いときは false。
bool IsInsideScreen(const BaseScene& scene, const Vector3& position, float ndcMargin);

// シーンの直下から、自機(PlayerComponent を持つオブジェクト)を探して返す。見つからなければ nullptr
std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer(const BaseScene& scene);

} // namespace EnemyShootUtil
