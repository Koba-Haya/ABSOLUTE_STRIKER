#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/camera/ITimelineCamera.h"
#include "Type/Vector.h"
#include "EnemyShootUtil.h"
#include "EnemyBulletLauncher.h"
#include <string>

namespace AbsoluteEngine {
class ISpawner;
class IScreenView;
class ISceneQuery;
}

class EnemyShootComponent : public AbsoluteEngine::IComponent {
public:
    // spawner: 弾を出す窓口。screen: 画面内か、島に隠れているかの判定に使う窓口。query: 自機を探す窓口。
    // bullets: 弾の定義の表(撃つ弾は、JSON の "bulletId" で選ぶ)。bulletPools: 弾の種類ごとの置き場所(シーンが持つ)。
    // いずれも所有しない。このコンポーネントより長く生きること
    EnemyShootComponent(AbsoluteEngine::ISpawner& spawner, AbsoluteEngine::IScreenView& screen, AbsoluteEngine::ISceneQuery& query,
                        const BulletCatalog& bullets, EnemyBulletPools& bulletPools)
        : spawner_(spawner), screen_(screen), query_(query), launcher_(bullets, bulletPools) {}
    ~EnemyShootComponent() override = default;

    void Update(float deltaTime) override;

    // レール座標の指定で出現したとき、進行方向の初期値を「レールの進行方向の逆(自機側を向く)」にする。
    // 動かない敵は、位置の変化から向きを推定できないので、この初期値のまま「正面の自機だけを撃つ」判定に使う。
    // 世界座標の指定で出現したときは呼ばれず、既定の向き(世界の -Z)のまま
    void OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) override;

    std::string GetTypeName() const override { return "EnemyShootComponent"; }

    // 設定(撃つ弾の id)の保存と読み込み
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;
    // エディタのインスペクタ(撃つ弾の選択)
    void DrawInspectorUI() override;

    // 発射間隔の設定
    void SetShootInterval(float interval) { shootInterval_ = interval; }
    float GetShootInterval() const { return shootInterval_; }

    // ゲームシーン側から発射タイミングを確認（ポーリング）するための機能
    bool WantToShoot() const { return wantToShoot_; }
    void ClearShootFlag() { wantToShoot_ = false; }

private:
    AbsoluteEngine::ISpawner& spawner_; // 弾を出す窓口
    AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置、画面内か)を知る窓口
    AbsoluteEngine::ISceneQuery& query_; // 自機などを探すために、シーンのオブジェクト一覧を調べる窓口
    EnemyBulletLauncher launcher_; // 撃つ弾の選択と、弾を出す役
    float shootInterval_ = 2.0f; // デフォルトは2秒に1回
    float shootTimer_ = 0.0f;
    EnemyShootUtil::IslandHideGate islandGate_; // 島に隠れている間は撃たない。出てきてから少し待つ
    bool wantToShoot_ = false;

    // 進行方向追跞用（マジックナンバー不要で動的に取得）
    Vector3 prevPos_     = { 0, 0, 0 }; // 前フレームの位置
    Vector3 facingDir_   = { 0, 0, -1 }; // 現在の進行方向（初期値：Z軍手前）
    bool hasPrevPos_     = false; // prevPos_が初期化済みかのフラグ
};
