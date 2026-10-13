#include "PlayerComponent.h"
#include "Input.h"
#include "GameCamera.h"
#include <algorithm>
#include <cmath>
#include <random>
#include "AbsoluteEngine/scene/BaseScene.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AbsoluteEngine/scene/RigidbodyComponent.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "ModelResource.h"
#include "ParticleManager.h"
#include "ReticleAim.h"
#include "Application/ScreenSize.h"
#include "../Bullet/BulletFactory.h"
#include "Logger.h"

// 武器（右手に持たせる仮モデル）の右手ボーン名
static const char* const kRightHandBoneName = "mixamorig:RightHand";

// 照準点（世界で狙う1点）：カメラからカーソルレイ上に kAimDistance 進んだ点
static constexpr float kAimDistance = 80.0f; // カメラから照準点までの距離（m）

// 3重レティクルの位置：発射基点から照準点へ向かう線を、この割合で分割した点に置く
// （0 = 発射基点、1 = 照準点）。遠レティクルは照準点そのものになる。
static constexpr float kNearReticleRatio = 0.17f; // 近レティクル
static constexpr float kMidReticleRatio  = 0.5f;  // 中レティクル
static constexpr float kFarReticleRatio  = 1.0f;  // 遠レティクル（弾道の照準）

// バンク角の最大値・補間速度（スターフォックス64風：素早く傾き、素早く戻る）
static constexpr float kMaxBankRoll  = 0.6f;  // 最大ロール角（ラジアン）：機敏な動きに合わせて強調
static constexpr float kMaxBankPitch = 0.15f; // 最大ピッチ追加角（ラジアン）
static constexpr float kBankSpeed    = 8.0f;  // バンク補間速度：素早く傾き、素早く戻るように倍速

// 回避行動（バレルロール）のパラメータ
static constexpr float kDodgeDuration     = 0.35f;  // 無敵＋ロール演出の時間（秒）
static constexpr float kDodgeCooldown     = 0.8f;   // 再発動までのクールダウン（秒）
static constexpr float kDodgeCursorOffset = 220.0f; // 回避時にカーソルを瞬間移動させる距離（ピクセル）
static constexpr float kTwoPi             = 6.28318530718f;

// 通常弾
static constexpr float kNormalBulletSpeed    = 50.0f;  // 弾速（m/秒）
static constexpr float kMinAimDistance       = 0.001f; // 照準方向を求められる最小距離
static constexpr float kNormalBulletRadius   = 0.5f;   // コライダー半径
static constexpr float kNormalShootInterval  = 0.15f;  // 連射間隔（秒）

// ホーミング弾
static constexpr float kHomingBulletRadius   = 1.0f;   // コライダー半径
static constexpr float kHomingBulletDuration = 0.6f;   // 着弾までの時間（秒）
static constexpr float kHomingSideOffsetMax  = 6.0f;   // ベジェ制御点の横ブレ幅（±）
static constexpr float kHomingUpOffsetMin    = 5.0f;   // ベジェ制御点の上方向オフセット（最小）
static constexpr float kHomingUpOffsetMax    = 10.0f;  // 同（最大）

void PlayerComponent::Initialize() {
  cursorPos_ = { ScreenSize::kCenterX, ScreenSize::kCenterY };
  localPos_  = { 0.0f, -0.5f };
  hp_        = maxHp_;
  bankRoll_  = 0.0f;
  bankPitch_ = 0.0f;
  dodgeTimer_ = 0.0f;
  dodgeCooldownTimer_ = 0.0f;
  dodgeRollSign_ = 1.0f;

  // 武器モデル（仮：cube.objを武器として持たせる）の生成
  if (!weaponModel_) {
      auto* am = AbsoluteEngine::AssetManager::GetInstance();
      auto weaponRes = am->Load<ModelResource>("resources/app/cube/cube.obj");
      if (weaponRes) {
          weaponModel_ = std::make_unique<ModelInstance>();
          ModelInstance::CreateInfo ci{};
          ci.resource = weaponRes;
          // 明るく目立つ色にする（レールシューティングでカメラが自機から離れているため、
          // 地味な灰色だと視認しづらい）
          ci.baseColor = { 0.95f, 0.25f, 0.15f, 1.0f };
          weaponModel_->Initialize(ci);
      }
  }
}

void PlayerComponent::Update(float deltaTime) {
  if (!owner_) return;
  auto scene  = BaseScene::GetActiveScene();
  if (!scene) return;
  auto* input  = scene->GetInput();
  auto* camera = scene->GetMainCamera();
  if (!input || !camera) return;

  // 自機の RigidbodyComponent が存在する場合は重力を無効化する
  // （物理演算によって自機が落下するバグの解消）
  if (auto rb = owner_->GetComponent<AbsoluteEngine::RigidbodyComponent>()) {
      rb->SetUseGravity(false);
      rb->SetKinematic(true); // 物理演算の一切を無効化して完全手動制御にする
  }

  const bool isPlaying = scene->GetPlayMode() == PlayMode::Play;

  // マウスはPlayモードでカーソルを閉じ込めている間だけ照準に使う
  const PlayerInputState in = PlayerInput::Poll(*input, isPlaying && MouseCapture::IsActive());

  MoveCursor_(in, deltaTime);

  // バンク角・回避方向に使う「今フレームの移動入力」。
  // マウス移動量は速度に換算し、キー/パッドと同じ -1〜1 の範囲に丸める。
  Vector2 moveDir = in.moveDir;
  if (deltaTime > 0.0f) {
      const float mouseToDir = 1.0f / (cursorSpeed_ * deltaTime);
      moveDir.x = std::clamp(moveDir.x + in.mouseDelta.x * mouseToDir, -1.0f, 1.0f);
      moveDir.y = std::clamp(moveDir.y + in.mouseDelta.y * mouseToDir, -1.0f, 1.0f);
  }

  UpdateBank_(moveDir, deltaTime);
  const float dodgeRollOffset = UpdateDodge_(in, moveDir, deltaTime, isPlaying);

  // カーソル方向の射線を1回だけ求め、自機の追従目標・弾道・レティクルで共用する
  const Vector3 cursorRayDir = ReticleAim::CursorRayDirection(*camera, cursorPos_);
  camDirCached_ = cursorRayDir; // 弾の発射方向に直接使う（t.translateのLerp遅れの影響を受けない）

  UpdateBodyTransform_(*camera, cursorRayDir, dodgeRollOffset, deltaTime);

  // 発射基点は1フレーム内で1回だけ求め、レティクル配置と弾の発射で同じ値を使う
  const Vector3 muzzlePos = GetHandPosition_();
  UpdateReticles_(*camera, cursorRayDir, muzzlePos);

  // ロックオン・弾発射はPlayMode::Playのときのみ処理する
  // （タイムラインのエディットモードプレビュー（レールカメラのシーク同期）でも
  //   このUpdate()は位置合わせのために呼ばれるため、ここをガードしないと
  //   Playモードにしていないのにクリック/スペース入力で実弾発射が走ってしまう）
  if (!isPlaying) return;

  lockon_.Update(deltaTime, in.fireHeld, camera, scene, cursorPos_);

  if (shootCooldown_ > 0.0f) {
      shootCooldown_ -= deltaTime;
  }

  UpdateFire_(in, scene, *camera, muzzlePos);
}

void PlayerComponent::MoveCursor_(const PlayerInputState& in, float deltaTime) {
  const float step = cursorSpeed_ * deltaTime;
  cursorPos_.x += in.moveDir.x * step + in.mouseDelta.x;
  cursorPos_.y += in.moveDir.y * step + in.mouseDelta.y;

  // カーソルのクランプ（画面全体を可動域とし、端まで届くように設定）
  cursorPos_.x = std::clamp(cursorPos_.x, 0.0f, ScreenSize::kWidth);
  cursorPos_.y = std::clamp(cursorPos_.y, 0.0f, ScreenSize::kHeight);
}

// 左右移動でロール、上下移動でピッチ上乗せ。原作ライクな機体傾きを実現。
void PlayerComponent::UpdateBank_(const Vector2& moveDir, float deltaTime) {
  const float targetRoll  = -moveDir.x * kMaxBankRoll;  // 右移動で右傾き（マイナス）
  const float targetPitch =  moveDir.y * kMaxBankPitch; // 上移動でピッチ下（前傾き）

  bankRoll_  += (targetRoll  - bankRoll_)  * kBankSpeed * deltaTime;
  bankPitch_ += (targetPitch - bankPitch_) * kBankSpeed * deltaTime;
}

// スターフォックス64風に、入力方向へカーソルを瞬間的にずらしつつ機体を
// 1回転（0→2π）させる。ロール角は2πでちょうど元の姿勢に戻るため、
// 演出が終わった瞬間に見た目上の違和感なく通常制御へ復帰する。
// 発動中（dodgeTimer_ > 0）は IsInvincible() が true になり、
// OnCollision 側の被ダメージ判定を無効化する。
float PlayerComponent::UpdateDodge_(const PlayerInputState& in, const Vector2& moveDir,
                                    float deltaTime, bool isPlaying) {
  if (dodgeCooldownTimer_ > 0.0f) dodgeCooldownTimer_ -= deltaTime;
  if (dodgeTimer_ > 0.0f) dodgeTimer_ -= deltaTime;

  if (isPlaying && in.dodgePressed && dodgeTimer_ <= 0.0f && dodgeCooldownTimer_ <= 0.0f) {
      float dodgeDirX = moveDir.x;
      float dodgeDirY = moveDir.y;
      const float dodgeDirLen = std::sqrt(dodgeDirX * dodgeDirX + dodgeDirY * dodgeDirY);
      if (dodgeDirLen > 0.0001f) {
          // 方向入力があればその方向へカーソルを瞬間移動させる
          dodgeDirX /= dodgeDirLen;
          dodgeDirY /= dodgeDirLen;
          cursorPos_.x = std::clamp(cursorPos_.x + dodgeDirX * kDodgeCursorOffset, 0.0f, ScreenSize::kWidth);
          cursorPos_.y = std::clamp(cursorPos_.y + dodgeDirY * kDodgeCursorOffset, 0.0f, ScreenSize::kHeight);
      } else {
          // 無入力時はその場でロールするだけ（位置は変えない）
          dodgeDirX = 1.0f;
      }

      dodgeRollSign_ = (dodgeDirX < 0.0f) ? -1.0f : 1.0f;
      dodgeTimer_ = kDodgeDuration;
      dodgeCooldownTimer_ = kDodgeCooldown;

      // 回避エフェクト（発光パーティクル）
      static std::mt19937 dodgeRng(std::random_device{}());
      std::uniform_real_distribution<float> dodgeDist(-2.0f, 2.0f);
      for (int i = 0; i < 10; ++i) {
          Vector3 vel = { dodgeDist(dodgeRng), dodgeDist(dodgeRng), dodgeDist(dodgeRng) };
          ParticleManager::GetInstance()->Emit(
              "default", owner_->GetTransform().translate, vel,
              Vector3{ 0.4f, 0.4f, 0.4f }, Vector3{ 0, 0, 0 }, 0.25f,
              Vector4{ 0.4f, 0.8f, 1.0f, 1.0f });
      }
  }

  if (dodgeTimer_ <= 0.0f) return 0.0f;
  const float dodgeProgress = 1.0f - (dodgeTimer_ / kDodgeDuration);
  return dodgeRollSign_ * kTwoPi * dodgeProgress;
}

// カーソルが画面上で指している3D位置を、カメラ正面 cameraDistance_ の平面上に
// 射影した Right/Up 成分を目標にして、Lerp で滑らかに追従する。
// これによりカーソルが画面端にあれば自機も画面端まで移動でき、
// cameraDistance_ を変更しても可動域が自動的に一致し続ける。
void PlayerComponent::UpdateBodyTransform_(const GameCamera& camera, const Vector3& cursorRayDir,
                                           float dodgeRollOffset, float deltaTime) {
  const Vector3 eye     = camera.GetEye();
  const Vector3 forward = camera.GetForward();
  const Vector3 right   = camera.GetRight();
  const Vector3 up      = camera.GetActualUp();

  // 平面の中心（カメラ正面）と、カーソルのレイが指す点
  const Vector3 basePos = {
      eye.x + forward.x * cameraDistance_,
      eye.y + forward.y * cameraDistance_,
      eye.z + forward.z * cameraDistance_
  };
  const Vector3 targetWorldPos = {
      eye.x + cursorRayDir.x * cameraDistance_,
      eye.y + cursorRayDir.y * cameraDistance_,
      eye.z + cursorRayDir.z * cameraDistance_
  };
  const Vector3 delta = {
      targetWorldPos.x - basePos.x,
      targetWorldPos.y - basePos.y,
      targetWorldPos.z - basePos.z
  };
  const float localTargetX = Dot(delta, right);
  const float localTargetY = Dot(delta, up);

  localPos_.x += (localTargetX - localPos_.x) * moveSpeed_ * deltaTime;
  localPos_.y += (localTargetY - localPos_.y) * moveSpeed_ * deltaTime;

  // ワールド座標: Eye + Forward*Dist + Right*localX + Up*localY
  auto& t = owner_->GetTransform();
  t.translate = {
      basePos.x + right.x * localPos_.x + up.x * localPos_.y,
      basePos.y + right.y * localPos_.x + up.y * localPos_.y,
      basePos.z + right.z * localPos_.x + up.z * localPos_.y
  };

  // 回転：カメラ向き（ヨー・ピッチ）＋バンク角（ロール・ピッチ追加）
  const float yaw   = std::atan2(forward.x, forward.z);
  const float xzLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
  const float pitch = std::atan2(-forward.y, xzLen);
  t.rotate = { pitch + bankPitch_, yaw, bankRoll_ + dodgeRollOffset };
}

// 「照準点」を1つだけ決め、レティクルと弾の両方をそこから派生させる。
//   照準点 A = カメラ位置 + カーソルレイ * kAimDistance（カーソルのレイ上なので、画面上で必ずカーソル位置に見える）
//   レティクル = 発射基点 M と A を結ぶ線上の点（遠レティクル = A）
// 弾も M から A へ飛ぶため、近・中・遠のどのレティクルも弾道上にあり、
// 自機の追従遅れや画面端でもレティクルと弾の方向が一致する。
// 画面上の視差（近・中・遠が一列に並ぶ見た目）は HUD 側の WorldToScreen（透視変換）が処理する。
void PlayerComponent::UpdateReticles_(const GameCamera& camera, const Vector3& cursorRayDir,
                                      const Vector3& muzzlePos) {
  const Vector3 eye = camera.GetEye();
  const Vector3 aimPoint = {
      eye.x + cursorRayDir.x * kAimDistance,
      eye.y + cursorRayDir.y * kAimDistance,
      eye.z + cursorRayDir.z * kAimDistance
  };
  auto place = [&](float ratio) -> Vector3 {
      return {
          muzzlePos.x + (aimPoint.x - muzzlePos.x) * ratio,
          muzzlePos.y + (aimPoint.y - muzzlePos.y) * ratio,
          muzzlePos.z + (aimPoint.z - muzzlePos.z) * ratio
      };
  };
  nearReticleWorldPos_ = place(kNearReticleRatio);
  midReticleWorldPos_  = place(kMidReticleRatio);
  farReticleWorldPos_  = place(kFarReticleRatio);
}

Vector3 PlayerComponent::GetHandPosition_() const {
  Vector3 handPos = owner_->GetTransform().translate;
  if (auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>()) {
      if (auto* instance = modelComp->GetModelInstance()) {
          Matrix4x4 handWorld = instance->GetBoneWorldMatrix(kRightHandBoneName);
          handPos = { handWorld.m[3][0], handWorld.m[3][1], handWorld.m[3][2] };
      }
  }
  return handPos;
}

void PlayerComponent::UpdateFire_(const PlayerInputState& in, BaseScene* scene,
                                  const GameCamera& camera, const Vector3& handPos) {
  if (!in.fireReleased) return;

  EmitMuzzleFlash_(handPos);

  if (lockon_.IsLockingMode()) {
      // ロックオンモード → ホーミング弾（ベジェ曲線）を全ターゲットに発射
      FireHomingBullets_(scene, camera, handPos);
  } else {
      FireNormalBullet_(scene, handPos);
  }
  lockon_.Initialize();
}

// 手からのマズルフラッシュ（パーティクル）
void PlayerComponent::EmitMuzzleFlash_(const Vector3& handPos) const {
  static std::mt19937 muzzleRng(std::random_device{}());
  std::uniform_real_distribution<float> muzzleDist(-1.0f, 1.0f);
  for (int i = 0; i < 6; ++i) {
      Vector3 vel = {
          muzzleDist(muzzleRng) * 3.0f,
          muzzleDist(muzzleRng) * 3.0f,
          muzzleDist(muzzleRng) * 3.0f
      };
      // レールシューティングでカメラが自機から離れているため、
      // 元の scale 0.3 / lifeTime 0.15 だと画面上でほぼ視認できなかった。
      // 視認性を上げるためスケールと寿命を引き上げる。
      ParticleManager::GetInstance()->Emit(
          "default", handPos, vel,
          Vector3{ 0.6f, 0.6f, 0.6f }, Vector3{ 0, 0, 0 }, 0.3f,
          Vector4{ 1.0f, 0.9f, 0.4f, 1.0f });
  }
}

void PlayerComponent::FireHomingBullets_(BaseScene* scene, const GameCamera& camera, const Vector3& handPos) {
  const auto& targets = lockon_.GetLockedTargets();
  if (targets.empty()) return;

  static std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> sideDist(-kHomingSideOffsetMax, kHomingSideOffsetMax);
  std::uniform_real_distribution<float> upDist(kHomingUpOffsetMin, kHomingUpOffsetMax);

  const Vector3 right = camera.GetRight();

  for (const auto& weakTarget : targets) {
      auto target = weakTarget.lock();
      if (!target) continue;

      const Vector3 p2Initial = target->GetTransform().translate;
      const Vector3 midPoint  = { (handPos.x + p2Initial.x) * 0.5f, (handPos.y + p2Initial.y) * 0.5f, (handPos.z + p2Initial.z) * 0.5f };

      const float sideOffset = sideDist(rng);
      const float upOffset   = upDist(rng);

      HomingBulletSpec spec;
      spec.startPos   = handPos;
      spec.controlPos = {
          midPoint.x + right.x * sideOffset,
          midPoint.y + upOffset,
          midPoint.z + right.z * sideOffset
      };
      spec.duration = kHomingBulletDuration;
      spec.radius   = kHomingBulletRadius;
      spec.target   = weakTarget;
      scene->AddRootObject(BulletFactory::CreatePlayerHoming(spec));
  }
}

// 通常弾：発射基点から「遠レティクル（= 照準点）」へ向けて発射する。
// 遠レティクルは UpdateReticles_ で発射基点と同じ値から求めているため、
// 近・中レティクルも必ずこの弾道上に載る。
void PlayerComponent::FireNormalBullet_(BaseScene* scene, const Vector3& handPos) {
  if (shootCooldown_ > 0.0f) return;
  shootCooldown_ = kNormalShootInterval;

  Vector3 aimDir = {
      farReticleWorldPos_.x - handPos.x,
      farReticleWorldPos_.y - handPos.y,
      farReticleWorldPos_.z - handPos.z
  };
  const float aimLen = std::sqrt(aimDir.x * aimDir.x + aimDir.y * aimDir.y + aimDir.z * aimDir.z);
  if (aimLen > kMinAimDistance) {
      aimDir = { aimDir.x / aimLen, aimDir.y / aimLen, aimDir.z / aimLen };
  } else {
      // 基点とレティクルがほぼ同位置で方向が定まらない場合は、カーソル方向で代用する
      aimDir = camDirCached_;
  }

  BulletSpec spec;
  spec.position = handPos;
  spec.velocity = {
      aimDir.x * kNormalBulletSpeed,
      aimDir.y * kNormalBulletSpeed,
      aimDir.z * kNormalBulletSpeed
  };
  spec.radius = kNormalBulletRadius;
  scene->AddRootObject(BulletFactory::CreatePlayerNormal(spec));
}

void PlayerComponent::Draw() {
  if (!owner_ || !weaponModel_) return;

  auto* modelComp = owner_->GetComponent<AbsoluteEngine::ModelComponent>();
  if (!modelComp) return;
  auto* instance = modelComp->GetModelInstance();
  if (!instance) return;

  // 武器（仮モデル）を毎フレーム右手ボーンに追従させる。
  // GetBoneWorldMatrix()が返す行列は、Mixamoリグのボーン階層に蓄積された
  // （スキニング目的の）非常に小さいスケール成分を含んでおり、そのまま使うと
  // 武器がほぼ見えないサイズまで縮んでしまう（座標自体は正しいため気づきにくい）。
  // そのため位置（平行移動成分）だけを取り出し、スケールは自前で組み直す。
  Matrix4x4 handWorld = instance->GetBoneWorldMatrix(kRightHandBoneName);
  Vector3 handPos = { handWorld.m[3][0], handWorld.m[3][1], handWorld.m[3][2] };
  Matrix4x4 finalWorld = MakeAffineMatrix({ 0.3f, 0.3f, 0.3f }, Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f }, handPos);
  weaponModel_->SetWorld(finalWorld);
  weaponModel_->Draw();
}

void PlayerComponent::TakeDamage(int damage) {
  hp_ -= damage;
  if (hp_ < 0) hp_ = 0;
}

bool PlayerComponent::IsDead() const {
  return hp_ <= 0;
}

void PlayerComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (IsDead() || IsInvincible()) return;

    if (other->GetName().find("Enemy") != std::string::npos || other->GetTag() == "Enemy" ||
        other->GetName().find("Boss")  != std::string::npos || other->GetTag() == "Boss") {
        TakeDamage(1);
        // 【調査用の一時ログ】「HPが見えないまま急に死ぬ」現象の原因特定用。
        // どの相手との接触で何回ダメージが入ったかをフレーム単位で記録する。
        // 原因が分かったら削除する。
        Logger::GetInstance().Log("[PlayerDamage] hit by name=%s tag=%s -> hp=%d/%d",
            other->GetName().c_str(), other->GetTag().c_str(), hp_, maxHp_);
    }
}
