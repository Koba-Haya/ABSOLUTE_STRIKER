#pragma once
#include <vector>
#include <memory>
#include "Type/Vector.h"

// 前方宣言
namespace AbsoluteEngine {
    class GameObject;
    class IScreenView;
    class ISceneQuery;
}

class LockOnSystem {
public:
    // ロックオンできる数の初期値(成長システムで SetMaxLockTargets により増やす予定)
    static constexpr int kDefaultMaxLockTargets = 3;
    // ロックオンの範囲の半径(スクリーンのピクセル)の初期値。カーソルからこの距離以内の敵を捕まえる。
    // 強化で広げられる(SetLockRadius)。判定(SearchAndLockTargets)と、範囲の輪の表示(HUD)の両方が、GetLockRadius の値を使う
    static constexpr float kDefaultLockRadiusPixels = 75.0f;

    LockOnSystem() = default;
    ~LockOnSystem() = default;

    void Initialize();
    
    // 長押し時間のカウントや敵の検索を行う。fireHeld は発射/ロックボタンを押している間 true
    void Update(float deltaTime, bool fireHeld, const AbsoluteEngine::IScreenView& screen, const AbsoluteEngine::ISceneQuery& query, const Vector2& cursorPos);

    bool IsLockingMode() const { return isLockingMode_; }
    // weak_ptrのリストを返す（呼び出し側でlock()して生存確認してから使うこと）
    const std::vector<std::weak_ptr<AbsoluteEngine::GameObject>>& GetLockedTargets() const { return lockedTargets_; }
    
    // ロックオンできる数の上限
    int GetMaxLockTargets() const { return maxLockTargets_; }
    // 上限を変える(成長システム用)。0 未満は 0 にする。Initialize(撃った後のリセット)では変わらない
    void SetMaxLockTargets(int maxTargets) { maxLockTargets_ = maxTargets < 0 ? 0 : maxTargets; }
    // ロックオンの範囲の半径(スクリーンのピクセル)
    float GetLockRadius() const { return lockRadius_; }
    // 範囲を変える(強化用)。0 未満は 0 にする。Initialize(撃った後のリセット)では変わらない
    void SetLockRadius(float radius) { lockRadius_ = radius < 0.0f ? 0.0f : radius; }

    // いま捕まえている数
    int GetLockedCount() const { return static_cast<int>(lockedTargets_.size()); }

    // HUDに渡すための、ロック中対象のスクリーン座標を計算して返す
    std::vector<Vector2> GetLockedScreenPositions(const AbsoluteEngine::IScreenView& screen) const;

private:
    // weak_ptrで保持することで、ターゲットが破棄された後も安全に参照できる
    std::vector<std::weak_ptr<AbsoluteEngine::GameObject>> lockedTargets_;
    int maxLockTargets_ = kDefaultMaxLockTargets; // ロックオンできる数の上限
    float lockRadius_ = kDefaultLockRadiusPixels; // ロックオンの範囲の半径(スクリーンのピクセル)

    float holdTimer_ = 0.0f;
    float lockOnThreshold_ = 0.2f; // 何秒長押ししたらロックオンモードに入るか
    bool isLockingMode_ = false;

    // 敵をスクリーン上に収めているか判定し、ロックする処理
    void SearchAndLockTargets(const AbsoluteEngine::IScreenView& screen, const AbsoluteEngine::ISceneQuery& query, const Vector2& cursorPos);
};

