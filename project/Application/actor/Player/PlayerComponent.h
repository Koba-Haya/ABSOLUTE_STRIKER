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
namespace AbsoluteEngine {
class ISpawner;
class IScreenView;
class ISceneQuery;
class IPlayState;
}

class PlayerComponent : public AbsoluteEngine::IComponent {
public:
  // 最大 HP の基本の値(強化で増える前)
  static constexpr int kBaseMaxHp = 5;

  // spawner: 弾を出す窓口。screen: 照準や追従に使う、カメラの位置・向き・行列を知る窓口。query: 敵を探す窓口(ロックオン用)。
  // playState: 再生状態を知る窓口。input: 入力。いずれも所有しない。このコンポーネントより長く生きること
  PlayerComponent(AbsoluteEngine::ISpawner& spawner, AbsoluteEngine::IScreenView& screen, AbsoluteEngine::ISceneQuery& query,
                  AbsoluteEngine::IPlayState& playState, Input& input)
      : spawner_(spawner), screen_(screen), query_(query), playState_(playState), input_(input) {}
  ~PlayerComponent() override = default;


  // ロックオンの状態を読み取り専用で取得する（HUD の表示など）。ロックの操作は PlayerComponent の中だけで行う
  const LockOnSystem& GetLockOn() const { return lockon_; }
  // ロックオンできる数の上限を変える(アイテムなどの強化用。ロックの操作そのものは、ここでは行わない)
  void SetMaxLockTargets(int maxTargets) { lockon_.SetMaxLockTargets(maxTargets); }
  // ロックオンの範囲の半径を変える(強化用)
  void SetLockRadius(float radius) { lockon_.SetLockRadius(radius); }
  // 最大 HP を変える(強化用)。増えた分だけ、HP も回復する。減ったときは、HP が最大 HP を超えないようにする
  void SetMaxHp(int maxHp);
  // 回避(バレルロール)のクールダウンを、seconds 秒だけ短くする(強化用。短縮しても、下限より短くならない)
  void SetDodgeCooldownReduction(float seconds) { dodgeCooldownReduction_ = seconds < 0.0f ? 0.0f : seconds; }
  // 通常弾の間隔の倍率(強化用。1 で通常、小さいほど連射が速い。下限より小さくならない)
  void SetFireIntervalScale(float scale);

  void Initialize();
  void Update(float deltaTime) override;
  void OnCollision(AbsoluteEngine::GameObject* other) override;

  std::string GetTypeName() const override { return "PlayerComponent"; }

  // ダメージを受ける。すでに死んでいる(HP 0)ときは無視する(死亡演出中に、重ねて呼ばれても、被弾の演出や二重の死亡が起きない)
  void TakeDamage(int damage);

  // 死亡演出中か。true の間は、カーソル追従・発射・ロックオンを止め、位置と向きは死亡演出(PlayerDeathDirector)が決める。
  // アニメーションの切り替え(PlayerAnimator)だけは進める
  void SetDying(bool dying) { dying_ = dying; }
  bool IsDying() const { return dying_; }
  // 蝶を、きりもみ(Tumble)で落ち始めさせる(死亡演出の、崩壊が始まる瞬間に呼ぶ)
  void PlayDeathTumble() { animator_.NotifyDeath(); }
  bool IsDead() const;
  int GetHp() const { return hp_; }
  int GetMaxHp() const { return maxHp_; }

  // 回避行動（バレルロール）中は無敵
  bool IsInvincible() const { return dodgeTimer_ > 0.0f; }

  // 演出のための無敵。true の間は、どんなダメージも受けない(ボス撃破の演出中に、残った敵や弾で死なないため)。
  // 回避の無敵(IsInvincible)とは別: こちらは TakeDamage 自体を止め、被弾のよろめきも出さない
  void SetInvulnerable(bool invulnerable) { invulnerable_ = invulnerable; }

  // 操作のロック。true の間は、キー・パッド・マウスのどれも受け付けない(何も操作していない状態にする)。
  // ステージ開始演出の間、自機が演出どおりに飛ぶよう、シーンが使う
  void SetInputLocked(bool locked) { inputLocked_ = locked; }
  // マウスで照準を動かすか(シーンが、カーソルを閉じ込めている間だけ true にする)
  void SetMouseAimEnabled(bool enabled) { mouseAimEnabled_ = enabled; }
  bool IsInputLocked() const { return inputLocked_; }

  // カーソル（照準）のスクリーン座標を取得
  const Vector2& GetCursorPos() const { return cursorPos_; }

  // 3D レティクルの各距離のワールド座標を取得（GameHUD・弾道計算に共有）
  const Vector3& GetNearReticleWorldPos() const { return nearReticleWorldPos_; }
  const Vector3& GetMidReticleWorldPos()  const { return midReticleWorldPos_; }
  const Vector3& GetFarReticleWorldPos()  const { return farReticleWorldPos_; }

private:
  AbsoluteEngine::ISpawner& spawner_; // 弾を出す窓口
  AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置・向き・行列)を知る窓口
  AbsoluteEngine::ISceneQuery& query_;  // 敵を探すために、シーンのオブジェクト一覧を調べる窓口(ロックオン用)
  AbsoluteEngine::IPlayState& playState_; // 再生状態(エディットか、プレイか)を知る窓口
  Input& input_;                        // 入力(キーボード、マウス、パッド)
  LockOnSystem lockon_;
  PlayerAnimator animator_; // 蝶のアニメーション(羽ばたき / 被弾)の切り替え

  // Update の各段階。呼び出し順は Update() を参照。
  void MoveCursor_(const PlayerInputState& in, float deltaTime);
  void UpdateBank_(const Vector2& moveDir, float deltaTime);
  // 回避の発動判定と進行。戻り値は今フレームのロール角オフセット（ラジアン）
  float UpdateDodge_(const PlayerInputState& in, const Vector2& moveDir, float deltaTime, bool isPlaying);
  // カーソルに追従して自機位置・姿勢を更新する
  void UpdateBodyTransform_(const Vector3& cursorRayDir, float dodgeRollOffset, float deltaTime);
  // 照準点（カーソルレイ上）と発射基点から、3重レティクルのワールド座標を求める
  void UpdateReticles_(const Vector3& cursorRayDir, const Vector3& muzzlePos);
  // 弾・リボンの尾・ミサイルの発射位置（muzzle ボーンの今のワールド位置。無ければ自機中心）
  Vector3 GetMuzzlePosition_() const;
  void UpdateFire_(const PlayerInputState& in, const Vector3& muzzlePos);
  void EmitMuzzleFlash_(const Vector3& muzzlePos) const;
  void FireHomingBullets_(const Vector3& muzzlePos);
  void FireNormalBullet_(const Vector3& muzzlePos);

  bool dying_ = false; // true の間は、操作・追従・発射を止める(SetDying)
  bool inputLocked_ = false; // true の間は操作を受け付けない(SetInputLocked)
  bool mouseAimEnabled_ = false; // マウスで照準を動かすか(SetMouseAimEnabled。シーンがカーソルを閉じ込めている間 true)
  bool invulnerable_ = false; // true の間はダメージを受けない(SetInvulnerable)
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

  int hp_ = kBaseMaxHp;
  int maxHp_ = kBaseMaxHp;
  float dodgeCooldownReduction_ = 0.0f; // 回避のクールダウンの短縮(秒。強化)
  float fireIntervalScale_ = 1.0f;      // 通常弾の間隔の倍率(強化)
  float shootCooldown_ = 0.0f;

  // 回避行動（バレルロール）：残り無敵時間・クールダウン・ロール方向（+1/-1）
  float dodgeTimer_ = 0.0f;
  float dodgeCooldownTimer_ = 0.0f;
  float dodgeRollSign_ = 1.0f;

};
