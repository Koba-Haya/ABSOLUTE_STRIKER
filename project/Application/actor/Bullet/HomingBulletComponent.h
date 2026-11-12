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

    /// ロック対象(追尾先)を返す。破棄済みなら空の weak_ptr が返る(lock() で生存確認すること)
    const std::weak_ptr<AbsoluteEngine::GameObject>& GetTarget() const { return target_; }

    /// <summary>
    /// このホーミング弾が、指定したオブジェクトを狙っているか(ロック対象か)を返す。
    /// </summary>
    /// <param name="victim">調べるオブジェクト(敵やボス)</param>
    /// <returns>ロック対象なら true。対象が破棄済みなら false</returns>
    bool IsAimingAt(const AbsoluteEngine::GameObject* victim) const;

    /// <summary>
    /// 弾のオブジェクトが、victim に当たってよいかを返す(敵・ボスの被弾判定用)。
    /// ホーミング弾は、ロック対象にしか当たらない。ホーミング弾でない弾は、常に当たってよい。
    /// </summary>
    /// <param name="bulletObj">当たってきた弾のオブジェクト</param>
    /// <param name="victim">当たられる側(敵やボス)</param>
    /// <returns>当たってよいなら true</returns>
    static bool CanHit(const AbsoluteEngine::GameObject& bulletObj, const AbsoluteEngine::GameObject* victim);
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
