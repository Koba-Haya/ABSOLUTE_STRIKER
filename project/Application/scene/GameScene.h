#pragma once
#include "BaseScene.h"
#include "GameCamera.h"
#include "Skybox.h"
#include "AbsoluteEngine/editor/EditorCamera.h"
#include "ModelInstance.h"
#include "ModelResource.h"
#include "Renderer.h"
#include "../actor/Player/PlayerComponent.h"
#include "../actor/Bullet/BulletComponent.h"
#include "../actor/Enemy/EnemyShootComponent.h"
#include "../hud/GameHUD.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "TextureResource.h"
#include "Animation.h"
#include "HitEffectSpawner.h"
#include "PostEffectDirector.h"
#include "ScoreTracker.h"
#include <memory>
#include <vector>
#include <string>

enum class GamePhase {
  InProgress,
  Boss,
  Clear,
  GameOver
};

class GameScene final : public BaseScene {
public:
  GameScene() = default;
  ~GameScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;
  void DrawEditorUI() override;

  // AddRootObject をオーバーライドして EnemyComponent へのコールバックを自動注入する（タスクC）
  // タイムライン経由でスポーンされた敵にも onDestroyed が登録されるようになる
  void AddRootObject(std::shared_ptr<AbsoluteEngine::GameObject> obj) override;

protected:
  // Debug Camera トグルがONの間だけ editorCamera_ の入力操作を許可する（タスクD）
  bool IsEditorCameraOperationEnabled() const override { return isDebugCamera_; }

  // Timeline Editor内のDebug CameraトグルボタンにisDebugCamera_のアドレスを渡す
  bool* GetDebugCameraFlag() override { return &isDebugCamera_; }

  // Draw() の描画カメラ分岐と一致させる（ギズモ/マウスピッキングが実際の見た目とズレないように）
  Camera* GetEditorViewCamera() const override;

private:
  // EnemyComponent::onDestroyed コールバックの登録を1箇所に集約する。
  // FormationMemberComponentを持つ場合は編隊トラッキングの初期カウントも行う。
  void RegisterEnemyCallbacks(const std::shared_ptr<AbsoluteEngine::GameObject>& obj);

  // --- Update の各段階 ---
  // プレイヤーの GameObject を探す（ロード時などでポインタが切り替わった場合に対応）
  std::shared_ptr<AbsoluteEngine::GameObject> FindPlayerObject_() const;
  // ゲームプレイ中の描画カメラ（デバッグカメラ or レールカメラ）
  Camera* GetGameplayCamera_() const;
  // ワールド座標をスクリーン座標(UV 0〜1)に変換する
  Vector2 WorldToScreenUv_(const Vector3& worldPos) const;
  // PostEffectDirector に渡す毎フレームの状態を集めて更新する
  void UpdatePostEffects_(float deltaTime);
  // Playモード中のみOSマウスカーソルを隠して閉じ込める（Escで一時解除）
  void UpdateMouseCapture_();
  // 寿命切れの爆発ライトと、プレイヤーより十分後方の敵を削除する
  void RemoveExpiredObjects_();
  void SpawnBoss_();
  // プレイヤー死亡 / ボス撃破でシーン遷移を要求する
  void CheckGameEnd_();
  void UpdateHud_();
  // ロックオンON/OFFに合わせてプレイヤーの歩行アニメーションをクロスフェードする
  void UpdatePlayerAnimation_(bool isLockingMode);

  // --- Draw の各段階 ---
  void DrawDebugRail_(Renderer* renderer) const;

private:
  std::unique_ptr<AbsoluteEngine::EditorCamera> debugCamera_;

  bool isDebugCamera_ = false;
  bool showDebugRail_ = true;
  bool showDebugSkeleton_ = false; // Bキーでトグル：プレイヤーの骨をデバッグ表示
  bool mouseCaptureSuspended_ = false; // Escで一時的にマウスの閉じ込めを解除している間 true

  Skybox skybox_;
  // 背景の雲海の見た目(ImGui で調整できる)。useCloudSea_ が false のときは従来のスカイボックスを描く(比較用)。
  // skybox_ は、雲海のときも環境マップ(反射)として使い続ける
  CloudSeaSettings cloudSea_;
  bool useCloudSea_ = true;

  // アクター関連
  std::shared_ptr<AbsoluteEngine::GameObject> playerObj_;

  // プレイヤー用スキニングモデル（歩行アニメーション）
  std::shared_ptr<Animation> playerWalkAnim_;
  // ロックオン中に切り替える別アニメーション（Animation補間の実演用）
  std::shared_ptr<Animation> playerSneakWalkAnim_;
  bool wasLockingMode_ = false;

  // ディゾルブのマスクテクスチャ
  std::shared_ptr<TextureResource> texNoise0_;

  HitEffectSpawner hitEffects_;    // 敵撃破時のヒットエフェクト
  PostEffectDirector postEffects_; // ポストエフェクト・カメラシェイクの発火とタイマー
  ScoreTracker scoreTracker_;      // スコア・撃破数・編隊の残数

  float time_ = 0.0f;

  GamePhase phase_ = GamePhase::InProgress;

  std::unique_ptr<GameHUD> gameHUD_;
};
