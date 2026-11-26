#include "EnemyShootComponent.h"
#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "AbsoluteEngine/scene/IScreenView.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../../actor/Player/PlayerComponent.h"
#include "../../actor/Player/PlayerFinder.h"
#include "GameCamera.h"  // VP行列取得・画面内判定のために必要
#include "TelegraphComponent.h"
#include <cmath>

void EnemyShootComponent::OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) {
    // レールの進行方向の逆 = 自機側へ向かう向き(世界の -Z 固定だと、レールが +Z 以外を向くと撃たなくなる)
    facingDir_ = { -frame.forward.x, -frame.forward.y, -frame.forward.z };
}

void EnemyShootComponent::Update(float deltaTime) {
    if (!GetOwner()) return;

    // --- 進行方向の動的追跡（マジックナンバーを使わずに向きを取得する）---
    Vector3 currentPos = GetOwner()->GetTransform().translate;
    if (hasPrevPos_) {
        Vector3 delta = {
            currentPos.x - prevPos_.x,
            currentPos.y - prevPos_.y,
            currentPos.z - prevPos_.z
        };
        float deltaLen = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        // 移動があった場合のみ方向を更新（停止中は直前の方向を維持）
        if (deltaLen > 0.001f) {
            facingDir_ = { delta.x / deltaLen, delta.y / deltaLen, delta.z / deltaLen };
        }
    }
    prevPos_    = currentPos;
    hasPrevPos_ = true;

    // 島の陰に隠れているかを毎フレーム追う(隠れている間は撃たない。出てきてから少し待つ)
    islandGate_.Update(screen_, currentPos, deltaTime);

    // タイマー更新
    shootTimer_ += deltaTime;
    // 予備動作(針の光): 撃つ時刻の手前から光り、撃つ時刻に終わる。予備動作の最中は撃たない
    auto* telegraph = GetOwner()->GetComponent<TelegraphComponent>();
    if (telegraph && telegraph->HoldFire(shootInterval_ - shootTimer_)) return;
    if (shootTimer_ < shootInterval_) return;
    shootTimer_ -= shootInterval_;
    if (telegraph) telegraph->End();
        
    // --- 画面内チェック（NDC ±1.2 マージン内にいる敵のみ発射を許可）---
    // カメラが無い、カメラの背後にいる、範囲外のときは発射しない（画面外ギリギリの敵も少し猶予を持たせる）
    constexpr float ndcMargin = 1.2f;
    if (!screen_.IsOnScreen(currentPos, ndcMargin)) return;

    // 島に隠れている(または、出てきたばかりの)敵は撃たない。隠れている間に来た発射の時刻は、捨てる
    if (!islandGate_.CanShoot()) return;

    // --- プレイヤーを探す ---
    const auto playerObj = PlayerFinder::FindPlayer(query_);
    if (!playerObj) return;

    // --- 視野角チェック（正面約30度以内のみ発射を許可）---
    Vector3 playerPos = playerObj->GetTransform().translate;
    Vector3 toPlayer  = {
        playerPos.x - currentPos.x,
        playerPos.y - currentPos.y,
        playerPos.z - currentPos.z
    };
    float toPlayerLen = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (toPlayerLen < 0.001f) return;
    toPlayer.x /= toPlayerLen;
    toPlayer.y /= toPlayerLen;
    toPlayer.z /= toPlayerLen;

    // 内積でプレイヤーが視野角内にいるか判定（0.866 ≈ cos(30°)）
    float dot = facingDir_.x * toPlayer.x + facingDir_.y * toPlayer.y + facingDir_.z * toPlayer.z;
    if (dot < 0.866f) return; // 正面30度以外は発射しない

    // --- 弾の発射 ---
    constexpr float kBulletSpeed = 20.0f;
    const Vector3 velocity = { toPlayer.x * kBulletSpeed, toPlayer.y * kBulletSpeed, toPlayer.z * kBulletSpeed };
    launcher_.Fire(spawner_, currentPos, velocity);
}

void EnemyShootComponent::Serialize(nlohmann::json& j) const {
    launcher_.Serialize(j);
}

void EnemyShootComponent::Deserialize(const nlohmann::json& j) {
    launcher_.Deserialize(j);
}

void EnemyShootComponent::DrawInspectorUI() {
    launcher_.DrawInspectorUI();
}



