#include "EnemyShootComponent.h"
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../../actor/Bullet/BulletFactory.h"
#include "../../actor/Player/PlayerComponent.h"
#include "GameCamera.h"  // VP行列取得・画面内判定のために必要
#include "TelegraphComponent.h"
#include <cmath>

void EnemyShootComponent::OnSpawnedOnRail(const AbsoluteEngine::RailFrame& frame) {
    // レールの進行方向の逆 = 自機側へ向かう向き(世界の -Z 固定だと、レールが +Z 以外を向くと撃たなくなる)
    facingDir_ = { -frame.forward.x, -frame.forward.y, -frame.forward.z };
}

void EnemyShootComponent::Update(float deltaTime) {
    if (!owner_) return;

    // --- 進行方向の動的追跡（マジックナンバーを使わずに向きを取得する）---
    Vector3 currentPos = owner_->GetTransform().translate;
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
    islandGate_.Update(currentPos, deltaTime);

    // タイマー更新
    shootTimer_ += deltaTime;
    // 予備動作(針の光): 撃つ時刻の手前から光り、撃つ時刻に終わる。予備動作の最中は撃たない
    auto* telegraph = owner_->GetComponent<TelegraphComponent>();
    if (telegraph && telegraph->HoldFire(shootInterval_ - shootTimer_)) return;
    if (shootTimer_ < shootInterval_) return;
    shootTimer_ -= shootInterval_;
    if (telegraph) telegraph->End();
        
    auto scene = BaseScene::GetActiveScene();
    if (!scene) return;

    // --- 画面内チェック（NDC ±1.2 マージン内にいる敵のみ発射を許可）---
    auto* camera = scene->GetMainCamera();
    if (!camera) return;

    Matrix4x4 viewMat = camera->GetViewMatrix();
    Matrix4x4 projMat = camera->GetProjectionMatrix();
    Matrix4x4 vpMat   = Multiply(viewMat, projMat);

    // 自身のワールド座標をクリップ空間に変換
    float w = currentPos.x * vpMat.m[0][3] + currentPos.y * vpMat.m[1][3]
            + currentPos.z * vpMat.m[2][3] + vpMat.m[3][3];
    if (w <= 0.01f) return; // カメラの背後にいる場合は発射しない

    float ndcX = (currentPos.x * vpMat.m[0][0] + currentPos.y * vpMat.m[1][0]
               + currentPos.z * vpMat.m[2][0] + vpMat.m[3][0]) / w;
    float ndcY = (currentPos.x * vpMat.m[0][1] + currentPos.y * vpMat.m[1][1]
               + currentPos.z * vpMat.m[2][1] + vpMat.m[3][1]) / w;

    // NDC ±1.2 の範囲外は発射しない（画面外ギリギリの敵も少し猶予を持たせる）
    constexpr float ndcMargin = 1.2f;
    if (ndcX < -ndcMargin || ndcX > ndcMargin || ndcY < -ndcMargin || ndcY > ndcMargin) return;

    // 島に隠れている(または、出てきたばかりの)敵は撃たない。隠れている間に来た発射の時刻は、捨てる
    if (!islandGate_.CanShoot()) return;

    // --- プレイヤーを探す ---
    std::shared_ptr<AbsoluteEngine::GameObject> playerObj = nullptr;
    for (const auto& obj : scene->GetRootObjects()) {
        if (obj && obj->GetComponent<PlayerComponent>()) {
            playerObj = obj;
            break;
        }
    }
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
    BulletSpec spec;
    spec.position = currentPos;
    spec.velocity = { toPlayer.x * kBulletSpeed, toPlayer.y * kBulletSpeed, toPlayer.z * kBulletSpeed };
    scene->AddRootObject(BulletFactory::CreateEnemy(spec));
}



