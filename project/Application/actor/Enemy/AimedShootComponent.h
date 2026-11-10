// ============================================================
// AimedShootComponent.h
// 役割: 自機へ向けて、一定の間隔で弾を撃つ(横切りながら撃つ敵など、動く向きと撃つ向きが別の敵用)。
//
// EnemyShootComponent との違い:
//   EnemyShootComponent は「位置の変化から進行方向を推定し、正面約30度に自機がいるときだけ撃つ」。
//   横切る敵は進行方向が横なので、自機がその方向に入らず、撃たない。
//   このコンポーネントは進行方向を使わず、画面内にいる間は、毎回、自機の位置を直接狙う。
//
// 設定(すべて JSON で調整できる。初期値は下の名前付き定数):
//   shootInterval          : 弾を撃つ間隔(秒)
//   firstShotDelay         : 画面内に入ってから、最初の1発までの待ち(秒)。入った瞬間に撃たれないようにする
//   bulletSpeed            : 弾の速さ(ユニット/秒)
//   aimErrorDegrees        : 狙いのばらつき(度)。0 なら自機の位置を正確に狙う。大きいほど、弾が自機から外れる
//   recoverAfterBulletGone : 直前の弾が消えて(撃ち落とされて)から、次の弾が出るまでの最低の間(秒)。
//                            撃ち落とした直後に、次の弾がすぐ出ないようにする
//   screenMargin           : 画面の端(NDC ±1)からの余裕。1.0 なら、画面内にいる間だけ撃つ
// ============================================================
#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include <memory>
#include <random>
#include <string>

namespace AbsoluteEngine {
class GameObject;
}

class AimedShootComponent : public AbsoluteEngine::IComponent {
public:
    AimedShootComponent();
    ~AimedShootComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "AimedShootComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // 狙いの向き dir(長さ1)に、最大 aimErrorDegrees_ 度のばらつきを足して返す
    Vector3 ApplyAimError_(const Vector3& dir);

    // 直前に撃った弾が、まだ生きているか
    bool IsLastBulletAlive_() const;

    // ---- 設定 ----
    float shootInterval_ = 2.8f;
    float firstShotDelay_ = 0.8f;
    float bulletSpeed_ = 14.0f;
    float aimErrorDegrees_ = 4.0f;
    float recoverAfterBulletGone_ = 1.0f;
    float screenMargin_ = 1.0f;

    // ---- 状態 ----
    float visibleTime_ = 0.0f;        // 画面内にいた時間の合計(最初の1発までの待ちに使う)
    bool hasShot_ = false;            // 1発でも撃ったか
    float sinceLastShot_ = 0.0f;      // 前の発射からの時間(画面内にいる間だけ進む)
    float sinceBulletGone_ = 0.0f;    // 前の弾が消えてからの時間(弾が生きている間は 0)
    std::weak_ptr<AbsoluteEngine::GameObject> lastBullet_; // 直前に撃った弾

    std::mt19937 random_; // 狙いのばらつき用(敵ごとに別の種で初期化する)
};
