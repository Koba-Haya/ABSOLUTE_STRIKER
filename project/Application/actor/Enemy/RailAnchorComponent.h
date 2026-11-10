// ============================================================
// RailAnchorComponent.h
// 役割: 敵の位置を、毎フレーム「レール座標」から求める(P2)。
//       レール座標 = カメラの今いる距離から、レールに沿って何ユニット先か(ahead)、
//       道の右へ何ユニット(lateral)、道の上へ何ユニット(vertical)。
//       敵は、世界のどこかに置かれたものではなく、「カメラについてくる道の上の点」に結びついて動く。
//         - レールが曲がっても、道に対する位置関係が保たれる(世界座標の直線では、曲がり角で道から外れる)
//         - 速さも、カメラに対する相対(ahead 方向の速さ 0 = カメラと同じ速さでついてくる)で決められる
//       横切りの敵は、lateral を一定の速さで変える。隊列の蛇行、島の裏から飛び出す動きは、
//       この「レール座標の速さ」を増やす形で足していく。
//
// 位置の持ち主: このコンポーネントは、毎フレーム owner の translate を上書きする。
//       StraightMoveComponent など、同じく位置を動かすコンポーネントとは、同じ敵に付けないこと(競合する)。
//
// 使い方: プレハブに付けて、タイムラインの SpawnEvent を「rail」(レール座標)で出現させる。
//       世界座標で出現した場合は OnSpawnedOnRail が呼ばれないので、何もしない(動かない)。
// ============================================================
#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/camera/ITimelineCamera.h"
#include <string>

class RailCameraComponent;

class RailAnchorComponent : public AbsoluteEngine::IComponent {
public:
    RailAnchorComponent() = default;
    ~RailAnchorComponent() override = default;

    void Update(float deltaTime) override;

    // レール座標で出現した直後に呼ばれる。出現位置から、いまの ahead / lateral / vertical を求めて、結びつきを始める。
    // lateral / vertical は、出現位置(編隊の散らしを含む)を、そのフレームの右・上へ射影して求める
    void OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) override;

    std::string GetTypeName() const override { return "RailAnchorComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // 現在のカメラのレール上の距離(弧長、ユニット)を返す。カメラが見つからなければ false
    bool GetCameraDistance_(float& outDistance) const;

    // シーンの直下から、レールカメラを探す(出現時に1回だけ)
    RailCameraComponent* FindRailCamera_() const;

    // ---- 設定(JSON で調整できる) ----
    // カメラに対する、ahead 方向の速さ(ユニット/秒)。0 なら、カメラと同じ速さでついてくる。正なら離れる、負なら近づく
    float aheadSpeed_ = 0.0f;
    // 道の右へ向かう速さ(ユニット/秒)。mirrorWithStartSide_ が true のときは「出現した側の反対へ向かう速さ」の意味になる
    float lateralSpeed_ = 0.0f;
    // 道の上へ向かう速さ(ユニット/秒)。mirrorWithStartSide_ が true のとき、縦は反転しない(横だけ)
    float verticalSpeed_ = 0.0f;
    // true なら、lateralSpeed_ の符号を出現した側に合わせて自動で決める(左で出れば右へ、右で出れば左へ、中央なら右へ)。
    // 左から来る敵と右から来る敵を、同じプレハブで作るための設定
    bool mirrorWithStartSide_ = false;
    // 出現してからの寿命(秒)。経過したら自分で消える。0 以下なら消えない
    // (カメラについてくる敵は、放っておくと永久に画面の周りに残るので、横切りは寿命を付ける)
    float lifetime_ = 0.0f;

    // ---- 状態(出現時に決まり、毎フレーム動く) ----
    bool anchored_ = false;           // 結びつき済みか(レール座標で出現したときだけ true)
    float aheadOffset_ = 0.0f;
    float lateral_ = 0.0f;
    float vertical_ = 0.0f;
    float lateralDirection_ = 1.0f;   // 横の動きの向き(mirrorWithStartSide_ で決まる +1 / -1)
    float elapsed_ = 0.0f;            // 出現してからの経過時間(秒)

    // 出現時に見つけたレールカメラ。シーンが所有する(敵よりシーンのほうが長く生きる)ので、生ポインタで持つ
    RailCameraComponent* railCamera_ = nullptr;
};
