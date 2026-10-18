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
  // 展示: 覆いが晴れたらすぐ、クリア / ゲームオーバーを交互に要求する(操作は受け付けない)
  void UpdateExhibitionFlow_(float deltaTime);
  // 開発用: プレイせずに、タイトル -> ゲーム -> クリア / ゲームオーバー -> タイトルの流れを確認するためのショートカット
  void CheckDebugFlowShortcut_();
  void UpdateHud_();
  // --- ゲーム開始のイントロ(画面遷移の覆いが晴れる間の、見た目だけの助走) ---
  // 覆いの中で始まったときだけ、イントロを始める。助走の基準になる位置(カメラ・自機)をここで記録する
  void BeginIntro_();
  // 覆いが晴れる進み具合に合わせて、カメラと自機を進行方向の後ろから進め、画角を落ち着かせる。
  // レール進行・敵・当たり判定は更新しない(晴れ終わるまで、ゲームの進行度は動かない)
  void UpdateIntro_(float deltaTime);
  // イントロを終える。カメラと自機を、基準の位置と画角へ正確に戻す
  void FinishIntro_();

  // --- Draw の各段階 ---
  void DrawDebugRail_(Renderer* renderer) const;

private:
  std::unique_ptr<AbsoluteEngine::EditorCamera> debugCamera_;

  bool isDebugCamera_ = false;
  bool showDebugRail_ = false;
  bool showDebugGrid_ = false; // 地面のグリッド線(デバッグ用)
  float exhibitionHoldTimer_ = 0.0f; // 展示: 通常の更新に入ってからの経過(秒)
  bool showDebugSkeleton_ = false; // Bキーでトグル：プレイヤーの骨をデバッグ表示
  bool mouseCaptureSuspended_ = false; // Escで一時的にマウスの閉じ込めを解除している間 true

  Skybox skybox_;
  // 背景の雲海の見た目(ImGui で調整できる)。useCloudSea_ が false のときは従来のスカイボックスを描く(比較用)。
  // skybox_ は、雲海のときも環境マップ(反射)として使い続ける
  CloudSeaSettings cloudSea_;
  bool useCloudSea_ = true;
  // ボリューム雲(ワールドに固定した箱の雲。ImGui で調整できる)。useVolumeCloud_ が false なら描かない(比較用)。
  // 描画の仕組みは Renderer::AddVolumeCloud と shaders/VolumeCloud.hlsli を参照
  VolumeCloudSettings volumeCloud_;
  bool useVolumeCloud_ = true;
  // 雲海を、どちらの方式で描くか(比較用。既定は新方式)。
  //   OldMethod: 旧方式(高さの地図を探す簡易版。空と雲を CloudSea.hlsli が 1 つのシェーダーで描く。cloudSea_)
  //   NewMethod: 新方式。空は SeaSky.PS.hlsl(seaSky_)、雲はボリューム雲の「層」(volumeCloudSea_)。
  //              箱の雲(volumeCloud_)と同じフレームに重ねて描ける(奥から手前の順)
  enum class SeaCloudMode : int { OldMethod = 0, NewMethod = 1 };
  SeaCloudMode seaCloudMode_ = SeaCloudMode::NewMethod;
  SeaSkySettings seaSky_;
  VolumeCloudSettings volumeCloudSea_ = MakeSeaLayerSettings();
  bool volumeCloudEditSea_ = true; // 調整 UI で、雲海の層(true)と箱(false)のどちらを調整するか

  // アクター関連
  std::shared_ptr<AbsoluteEngine::GameObject> playerObj_;

  // ディゾルブのマスクテクスチャ
  std::shared_ptr<TextureResource> texNoise0_;

  HitEffectSpawner hitEffects_;    // 敵撃破時のヒットエフェクト
  PostEffectDirector postEffects_; // ポストエフェクト・カメラシェイクの発火とタイマー
  ScoreTracker scoreTracker_;      // スコア・撃破数・編隊の残数

  float time_ = 0.0f;

  // イントロの状態
  bool introActive_ = false;
  Vector3 introBaseEye_{0.0f, 0.0f, 0.0f};    // 助走の基準(スタート位置)のカメラの位置
  Vector3 introBaseTarget_{0.0f, 0.0f, 1.0f}; // 同じく注視点
  Vector3 introBasePlayerPosition_{0.0f, 0.0f, 0.0f};
  Vector3 introBasePlayerRotation_{0.0f, 0.0f, 0.0f}; // 助走の基準の、自機の向き(昇りの機首上げの前の値)
  float introRailSpeed_ = 0.0f;               // 助走の速さに使う、レールの進む速さ

  GamePhase phase_ = GamePhase::InProgress;

  std::unique_ptr<GameHUD> gameHUD_;
};
