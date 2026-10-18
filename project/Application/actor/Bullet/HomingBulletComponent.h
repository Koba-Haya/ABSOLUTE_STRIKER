#pragma once
#include "Type/Vector.h"
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include <string>
#include <memory>

/// <summary>
/// 2次ベジェ曲線を用いたロックオン追尾弾コンポーネント。
/// 物理演算・Lerpによる速度操作を廃止し、
/// B(t) = (1-t)^2 * P0 + 2*(1-t)*t * P1 + t^2 * P2
/// の公式で座標を直接計算することで、絶対必中かつ豪華な放物線軌道を実現する。
/// </summary>
class HomingBulletComponent : public AbsoluteEngine::IComponent {
public:
    HomingBulletComponent() = default;
    ~HomingBulletComponent() override = default;

    /// <summary>
    /// ベジェ曲線の始点・制御点・着弾時間・ターゲットで初期化する。
    /// </summary>
    /// <param name="p0">始点（発射時の銃口ワールド座標）</param>
    /// <param name="p1">制御点（発射時に1回だけ計算した中継点）</param>
    /// <param name="duration">着弾までの総時間（秒）</param>
    /// <param name="target">追尾対象（weak_ptrでダングリングポインタを防ぐ）</param>
    void Initialize(const Vector3& p0, const Vector3& p1, float duration,
        std::weak_ptr<AbsoluteEngine::GameObject> target);

    void Update(float deltaTime) override;
    void OnCollision(AbsoluteEngine::GameObject* other) override;

    std::string GetTypeName() const override { return "HomingBulletComponent"; }

    bool IsActive() const { return isActive_; }
    void Deactivate() { isActive_ = false; }
    float GetCollisionRadius() const { return radius_; }

private:
    /// <summary>
    /// 弾が消えるときの共通処理。尾を切り離して残し、弾自体を破棄する。
    /// </summary>
    void Finish_();

    // --- ベジェ曲線用パラメータ ---
    Vector3 startPos_{ 0, 0, 0 };    // P0: 発射時の始点（固定）
    Vector3 controlPos_{ 0, 0, 0 };  // P1: 制御点（固定）
    float   progress_ = 0.0f;        // t: 進行度（0.0 → 1.0）
    float   duration_ = 0.6f;        // 着弾までの総時間（秒）

    // --- ターゲット ---
    // weak_ptrで保持することで、対象が破棄された後も安全にアクセスできる
    std::weak_ptr<AbsoluteEngine::GameObject> target_;

    // --- 弾のステータス ---
    bool  isActive_ = false;
    float radius_ = 1.0f;  // ホーミング弾は当たり判定を広めに

    // --- 前フレームの座標（回転計算用） ---
    Vector3 prevPos_{ 0, 0, 0 };
};
