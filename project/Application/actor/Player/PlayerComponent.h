#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/graphics/3d/model/ModelInstance.h"
#include "Type/Vector.h"
#include "LockOnSystem.h"
#include "PlayerInput.h"
#include "PlayerAnimator.h"
#include <memory>

class Input;
class GameCamera;
class BaseScene;

class PlayerComponent : public AbsoluteEngine::IComponent {
public:
  PlayerComponent() = default;
  ~PlayerComponent() override = default;


  // ロックオンの状態を読み取り専用で取得する（HUD の表示など）。ロックの操作は PlayerComponent の中だけで行う
  const LockOnSystem& GetLockOn() const { return lockon_; }

  void Initialize();
  void Update(float deltaTime) override;
  void OnCollision(AbsoluteEngine::GameObject* other) override;

  std::string GetTypeName() const override { return "PlayerComponent"; }

  void TakeDamage(int damage);
  bool IsDead() const;
  int GetHp() const { return hp_; }
  int GetMaxHp() const { return maxHp_; }

  // 回避行動（バレルロール）中は無敵
  bool IsInvincible() const { return dodgeTimer_ > 0.0f; }

  // 操作のロック。true の間は、キー・パッド・マウスのどれも受け付けない(何も操作していない状態にする)。
  // ステージ開始演出の間、自機が演出どおりに飛ぶよう、シーンが使う
  void SetInputLocked(bool locked) { inputLocked_ = locked; }
  bool IsInputLocked() const { return inputLocked_; }

  // カーソル（照準）のスクリーン座標を取得
  const Vector2& GetCursorPos() const { return cursorPos_; }

  // 3D レティクルの各距離のワールド座標を取得（GameHUD・弾道計算に共有）
  const Vector3& GetNearReticleWorldPos() const { return nearReticleWorldPos_; }
  const Vector3& GetMidReticleWorldPos()  const { return midReticleWorldPos_; }
  const Vector3& GetFarReticleWorldPos()  const { return farReticleWorldPos_; }

private:
  LockOnSystem lockon_;
  PlayerAnimator animator_; // 蝶のアニメーション(羽ばたき / 被弾)の切り替え

  // Update の各段階。呼び出し順は Update() を参照。
  void MoveCursor_(const PlayerInputState& in, float deltaTime);
  void UpdateBank_(const Vector2& moveDir, float deltaTime);
  // 回避の発動判定と進行。戻り値は今フレームのロール角オフセット（ラジアン）
  float UpdateDodge_(const PlayerInputState& in, const Vector2& moveDir, float deltaTime, bool isPlaying);
  // カーソルに追従して自機位置・姿勢を更新する
  void UpdateBodyTransform_(const GameCamera& camera, const Vector3& cursorRayDir, float dodgeRollOffset, float deltaTime);
  // 照準点（カーソルレイ上）と発射基点から、3重レティクルのワールド座標を求める
  void UpdateReticles_(const GameCamera& camera, const Vector3& cursorRayDir, const Vector3& muzzlePos);
  // 弾・リボンの尾・ミサイルの発射位置（muzzle ボーンの今のワールド位置。無ければ自機中心）
  Vector3 GetMuzzlePosition_() const;
  void UpdateFire_(const PlayerInputState& in, BaseScene* scene, const GameCamera& camera, const Vector3& muzzlePos);
  void EmitMuzzleFlash_(const Vector3& muzzlePos) const;
  void FireHomingBullets_(BaseScene* scene, const GameCamera& camera, const Vector3& muzzlePos);
  void FireNormalBullet_(BaseScene* scene, const Vector3& muzzlePos);

  bool inputLocked_ = false; // true の間は操作を受け付けない(SetInputLocked)
  Vector2 cursorPos_{ 640.0f, 360.0f }; // カーソルのスクリーン座標
  float cursorSpeed_ = 1000.0f;          // カーソルの移動速度（ピクセル/秒）：キビキビした操作感のため引き上げ

  Vector2 localPos_{ 0.0f, -0.5f }; // 自機の目標ローカル座標への追従用
  float moveSpeed_ = 4.0f;          // 追従速度（Lerpの係数）：スターフォックス64風に大幅引き上げ
  float cameraDistance_ = 22.0f;    // カメラから自機までの前方距離

  // バンク角制御（原作ライクなロール・ピッチ傾き）
  float bankRoll_  = 0.0f; // 現在のロール角（ラジアン）
  float bankPitch_ = 0.0f; // 現在のピッチ上乗せ角（ラジアン）

  // 3D レティクルの各距離のワールド座標（毎フレーム計算してキャッシュ）
  Vector3 nearReticleWorldPos_{ 0, 0, 0 };
  Vector3 midReticleWorldPos_ { 0, 0, 0 };
  Vector3 farReticleWorldPos_ { 0, 0, 0 };

  // カーソル方向のカメラ射線ベクトルキャッシュ（毎フレーム更新）
  // 通常弾は発射基点→遠レティクルへ飛ばすため、これは基点と照準点が
  // 重なって方向が定まらない場合のフォールバックにのみ使う。
  Vector3 camDirCached_{ 0, 0, 1 }; // 初期値は正面方向

  int hp_ = 5;
  int maxHp_ = 5;
  float shootCooldown_ = 0.0f;

  // 回避行動（バレルロール）：残り無敵時間・クールダウン・ロール方向（+1/-1）
  float dodgeTimer_ = 0.0f;
  float dodgeCooldownTimer_ = 0.0f;
  float dodgeRollSign_ = 1.0f;

};
