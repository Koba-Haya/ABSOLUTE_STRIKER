#include "LockOnSystem.h"
#include "AbsoluteEngine/scene/IScreenView.h"
#include "Application/ScreenSize.h"
#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "LockOnTarget.h"
#include "../Bullet/HomingBulletComponent.h"
#include "Application/occlusion/OcclusionWorld.h"
#include <algorithm>
#include <cmath>

namespace {
// カメラから敵までの視線が、遮蔽物に遮られているか。ロックオンは、島と雲(将来)のどちらに遮られても狙えない
// (島: 見えない。雲: 見えるが、ロックオンはできない、という決定)。通常弾の判定とは、遮る種類が違う
bool IsHiddenFromCamera(const Vector3& eye, const Vector3& targetPos) {
    return OcclusionWorld::GetInstance().IsSegmentBlocked(eye, targetPos, OccluderKind::kAll);
}
} // namespace

void LockOnSystem::Initialize() {
    lockedTargets_.clear();
    holdTimer_ = 0.0f;
    isLockingMode_ = false;
}

void LockOnSystem::Update(float deltaTime, bool fireHeld, const AbsoluteEngine::IScreenView& screen, const AbsoluteEngine::ISceneQuery& query, const Vector2& cursorPos) {
    if (!screen.HasCamera()) return;

    // 既に死んでいる・または破棄されている・島や雲に隠れた敵をロックオン対象から外す
    const Vector3 eye = screen.GetEye();
    lockedTargets_.erase(std::remove_if(lockedTargets_.begin(), lockedTargets_.end(),
        [&eye](const std::weak_ptr<AbsoluteEngine::GameObject>& weak) {
            // weak_ptr.lock()で生存確認（破棄済みなら自動でtrue=除外）
            auto obj = weak.lock();
            if (!obj) return true;
            // 死んでいる(敵、ボス、玉)、または、狙えなくなった(撃ち落とされた玉など)ものは、外す
            if (!LockOnTarget::IsAlive(*obj)) return true;
            // すでにロックしていた敵が、島の陰に入ったら、ロックを外す。
            // 理由: (1) 見えない敵に枠が残ると、隠れている位置が分かってしまう。(2) そのまま撃つと、ロック弾は島に当たって消えるので、
            // 枠を 1 つ無駄にする。外した枠は空くので、別の敵を捕まえられる。敵が島の陰から出てきて、カーソルの範囲に入れば、
            // 長押しのまま、また捕まえられる(検索は長押しの間、毎フレーム行われる)
            if (IsHiddenFromCamera(eye, obj->GetTransform().translate)) return true;
            return false;
        }), lockedTargets_.end());

    if (fireHeld) {
        holdTimer_ += deltaTime;
        if (holdTimer_ >= lockOnThreshold_) {
            isLockingMode_ = true;
            // 敵を検索してロックオン
            SearchAndLockTargets(screen, query, cursorPos);
        }
    } else {
        // ボタンを離した瞬間はPlayerComponent側で発射の判定に使われるため
        // ここで holdTimer_ と isLockingMode_ をリセットしてはいけない
        // (PlayerComponent::Update 側で発射処理後に lockon_.Initialize() を呼んでリセットする)
    }
}

void LockOnSystem::SearchAndLockTargets(const AbsoluteEngine::IScreenView& screen, const AbsoluteEngine::ISceneQuery& query, const Vector2& cursorPos) {
    if (GetLockedCount() >= maxLockTargets_) return; // 既に上限

    Matrix4x4 viewMat = screen.GetViewMatrix();
    Matrix4x4 projMat = screen.GetProjectionMatrix();
    Matrix4x4 vpMat = Multiply(viewMat, projMat);

    const auto& objects = query.Objects();

    // すでにロックオン弾(ホーミング弾)が飛んでいる敵は、対象にしない(1体の敵にロックオンは1回だけ)。
    // 撃ったあとは lockedTargets_ が空になるため、これがないと、弾の飛行中に同じ敵をもう一度ロックできてしまう
    std::vector<const AbsoluteEngine::GameObject*> aimedTargets;
    for (const auto& obj : objects) {
        if (!obj) continue;
        const auto* homing = obj->GetComponent<HomingBulletComponent>();
        if (!homing || !homing->IsActive()) continue;
        if (const auto aimed = homing->GetTarget().lock()) aimedTargets.push_back(aimed.get());
    }

    for (const auto& obj : objects) {
        if (!obj) continue;
        
        // 敵、ボス、ボスの誘導弾の玉のみ対象。判定はタグだけで行う(LockOnTarget::IsTargetTag)。
        // 名前の部分一致を使うと、敵弾(名前が EnemyBullet)まで対象になり、ロックオンの枠を取られてしまう。
        // 敵のプレハブはすべてタグ Enemy、ボスは GameScene が生成時にタグ Boss、玉は生成時にタグ EnemyOrb を付ける
        if (!LockOnTarget::IsTargetTag(obj->GetTag())) {
            continue;
        }

        // 死んでいない(玉は、現れきっていて、撃ち落とされていない)かチェック
        if (!LockOnTarget::IsAlive(*obj)) continue;

        // 既にロックオン済みかチェック（weak_ptr経由で比較）
        bool alreadyLocked = std::any_of(lockedTargets_.begin(), lockedTargets_.end(),
            [&obj](const std::weak_ptr<AbsoluteEngine::GameObject>& weak) {
                return weak.lock() == obj;
            });
        if (alreadyLocked) continue;

        // 飛行中のロックオン弾に狙われている敵は、対象にしない
        if (std::find(aimedTargets.begin(), aimedTargets.end(), obj.get()) != aimedTargets.end()) continue;

        // スクリーン内にいるか判定
        Vector3 worldPos = obj->GetTransform().translate;
        Vector3 posV = {
            worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0],
            worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1],
            worldPos.x * vpMat.m[0][2] + worldPos.y * vpMat.m[1][2] + worldPos.z * vpMat.m[2][2] + vpMat.m[3][2]
        };
        float w = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];

        if (w > 0.01f) { // カメラの前面にいる
            float ndcX = posV.x / w;
            float ndcY = posV.y / w;
            
            // 画面内かつカーソルに近いか判定
            float sx = (ndcX + 1.0f) * 0.5f * ScreenSize::kWidth;
            float sy = (1.0f - ndcY) * 0.5f * ScreenSize::kHeight;
            
            float dx = sx - cursorPos.x;
            float dy = sy - cursorPos.y;
            float distSq = dx * dx + dy * dy;
            
            // 範囲の半径(kLockRadiusPixels)以内の場合ロックオン
            if (distSq <= lockRadius_ * lockRadius_) {
                // カメラ(自機の視点)から敵までの線分が、島や雲に遮られているなら、対象にしない(見えない敵は狙えない)。
                // 画面の判定を通ったものだけに行うので、遮蔽の問い合わせは、カーソルの近くの敵の数だけで済む
                if (IsHiddenFromCamera(screen.GetEye(), worldPos)) continue;

                // shared_ptr → weak_ptr に変換して安全に保持
                lockedTargets_.push_back(obj);
                
                if (GetLockedCount() >= maxLockTargets_) {
                    break;
                }
            }
        }
    }
}

std::vector<Vector2> LockOnSystem::GetLockedScreenPositions(const AbsoluteEngine::IScreenView& screen) const {
    std::vector<Vector2> positions;
    if (!screen.HasCamera()) return positions;

    Matrix4x4 viewMat = screen.GetViewMatrix();
    Matrix4x4 projMat = screen.GetProjectionMatrix();
    Matrix4x4 vpMat = Multiply(viewMat, projMat);

    for (const auto& weak : lockedTargets_) {
        // weak_ptr.lock()で生存確認（破棄済みオブジェクトへのアクセスを防ぐ）
        auto obj = weak.lock();
        if (!obj) continue;
        Vector3 worldPos = obj->GetTransform().translate;
        Vector3 posV = {
            worldPos.x * vpMat.m[0][0] + worldPos.y * vpMat.m[1][0] + worldPos.z * vpMat.m[2][0] + vpMat.m[3][0],
            worldPos.x * vpMat.m[0][1] + worldPos.y * vpMat.m[1][1] + worldPos.z * vpMat.m[2][1] + vpMat.m[3][1],
            worldPos.x * vpMat.m[0][2] + worldPos.y * vpMat.m[1][2] + worldPos.z * vpMat.m[2][2] + vpMat.m[3][2]
        };
        float w = worldPos.x * vpMat.m[0][3] + worldPos.y * vpMat.m[1][3] + worldPos.z * vpMat.m[2][3] + vpMat.m[3][3];

        if (w > 0.001f) {
            float ndcX = posV.x / w;
            float ndcY = posV.y / w;
            
            // NDC(-1〜1) から スクリーン座標(0〜ScreenSize) への変換
            float sx = (ndcX + 1.0f) * 0.5f * ScreenSize::kWidth;
            float sy = (1.0f - ndcY) * 0.5f * ScreenSize::kHeight;
            
            positions.push_back({sx, sy});
        }
    }
    return positions;
}
