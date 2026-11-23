// ============================================================
// EnemyShootUtil.h
// 役割: 敵の射撃コンポーネントが共通で使う判定(画面内か、自機はどこか、島に隠れていないか)。
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
class IScreenView;
} // namespace AbsoluteEngine

namespace EnemyShootUtil {

// 島の陰から出てきた敵が、撃ち始めるまでの待ち時間(秒)。出てきた瞬間に撃たれると、見えてすぐ被弾する理不尽な形になるため、
// 見えてから少し間を置く。長くしすぎると、島から出ても撃たない敵に見える
constexpr float kRevealShotDelay = 0.6f;

// 世界座標 position の敵が、島の陰に隠れているか(screen のカメラから position までの線分が島に遮られるか)を返す。
// 見えない所から撃たれるのは理不尽なので、隠れている間は撃たない。雲は含めない(雲の裏からは撃ってよい)。
// カメラが無い、島の遮蔽物が登録されていないときは false。島の上に乗るタレットは、島の円柱より上にいるので false になる
bool IsHiddenByIsland(const AbsoluteEngine::IScreenView& screen, const Vector3& position);

// 「島に隠れている間は撃たない。出てきてから kRevealShotDelay 待つ」を、射撃コンポーネントごとに持つ小さな部品。
// 使い方: 毎フレーム Update(screen, 位置, deltaTime) を呼び、撃ってよいかの判定のところで CanShoot() を見る。
// 一度も隠れなかった敵は、最初から撃ってよい(待ち時間は、隠れたあとの出てきたときだけ)
class IslandHideGate {
public:
    // 毎フレーム呼ぶ。screen はメインカメラの見え方。position は敵の世界座標。隠れていれば待ちをやり直し、見えていれば経過時間を進める
    void Update(const AbsoluteEngine::IScreenView& screen, const Vector3& position, float deltaTime);
    // 撃ってよいか(隠れておらず、出てきてから待ち時間が過ぎている)
    bool CanShoot() const { return visibleTime_ >= kRevealShotDelay; }

private:
    float visibleTime_ = kRevealShotDelay; // 見えている時間。初期値は待ち済みの状態
};

// シーンの直下から、自機(PlayerComponent を持つオブジェクト)を探して返す。見つからなければ nullptr
std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer(const BaseScene& scene);

} // namespace EnemyShootUtil
