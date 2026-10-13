#define NOMINMAX
#include "GameScene.h"
#include "Renderer.h"
#include "DirectXCommon.h"
#include "DirectXResourceUtils.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "particle/GPUParticleManager.h"
#include "Input.h"
#include "../camera/RailCameraComponent.h"
#include "Spline.h"
#include "Method.h"
#include "GameObject.h"
#include <algorithm>
#include <cmath>
#include "Logger.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "AbsoluteEngine/scene/ComponentFactory.h"
#include "AbsoluteEngine/scene/timeline/TimelineEventFactory.h"
#include "AbsoluteEngine/editor/EditorUIManager.h"
#include "../actor/Enemy/StraightMoveComponent.h"
#include "../actor/Enemy/EnemyComponent.h"
#include "../actor/Enemy/EnemyShootComponent.h"
#include "../actor/Enemy/BossComponent.h"
#include "../actor/Enemy/FormationMemberComponent.h"
#include "../actor/Enemy/FormationSpawnEvent.h"
#include "../actor/Enemy/TurretShootComponent.h"
#include "../actor/Enemy/ChargeOnApproachComponent.h"
#include "../actor/Enemy/RamAttackComponent.h"
#include "../actor/Player/PlayerInput.h"
#include "../ScreenSize.h"
#include "CloudSeaDebugUi.h"
#include "SceneIds.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AnimationManager.h"
#include "AbsoluteEngine/scene/LightNodeComponent.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "graphics/particle/EffectManager.h"
#include "component/ExplosionLightComponent.h"

void GameScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);

  auto* am = AbsoluteEngine::AssetManager::GetInstance();

  hitEffects_.Initialize();

  // ディゾルブのマスクテクスチャのロード
  texNoise0_ = am->Load<TextureResource>("resources/noise/noise0.png");
  // ディゾルブのマスクテクスチャをレンダラーにセット
  if (texNoise0_) {
      Renderer::GetInstance()->SetDissolveMaskTexture(texNoise0_);
  }

  skybox_.Initialize("resources/app/dds/dds.dds");

  // パーティクルグループの初期化（連射ヒット時のアサートクラッシュ防止）
  ParticleManager::GetInstance()->CreateParticleGroup("default", "resources/app/particle/circle.png", 500);
  ParticleManager::GetInstance()->CreateParticleGroup("explosion", "resources/explosion/explosion.png", 100);

  // レールカメラの初期化
  auto camera = std::make_shared<GameCamera>();
  camera->Initialize();
  camera->SetPerspective(0.45f, Renderer::GetInstance()->GetAspectRatio(), 0.1f, 1000.0f);
  SetMainCamera(camera);

  // デバッグカメラ初期化（EditorCameraを、右ドラッグ不要で常時動ける自由視点モードで使う）
  debugCamera_ = std::make_unique<AbsoluteEngine::EditorCamera>();
  debugCamera_->Initialize();
  debugCamera_->SetPerspective(0.45f, Renderer::GetInstance()->GetAspectRatio(), 0.1f, 1000.0f);
  debugCamera_->SetRequireMouseForMovement(false);
  debugCamera_->SetKeyboardRotationEnabled(true);
  debugCamera_->SetMovementSpeeds(0.5f, 0.02f, 0.005f);

  AbsoluteEngine::ComponentFactory::GetInstance().Register("RailCameraComponent", []() {
      auto comp = std::make_unique<RailCameraComponent>();
      return comp;
  });
  // コンポーネントファクトリの登録
  AbsoluteEngine::ComponentFactory::GetInstance().Register("PlayerComponent", []() {
      auto comp = std::make_unique<PlayerComponent>();
      comp->Initialize();
      return comp;
  });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("BulletComponent", []() { return std::make_unique<BulletComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("StraightMoveComponent", []() { return std::make_unique<StraightMoveComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("EnemyComponent", []() { return std::make_unique<EnemyComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("EnemyShootComponent", []() { return std::make_unique<EnemyShootComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("BossComponent", []() { return std::make_unique<BossComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("TurretShootComponent", []() { return std::make_unique<TurretShootComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("ChargeOnApproachComponent", []() { return std::make_unique<ChargeOnApproachComponent>(); });
  AbsoluteEngine::ComponentFactory::GetInstance().Register("RamAttackComponent", []() { return std::make_unique<RamAttackComponent>(); });

  // タイムラインイベントファクトリの登録（編隊敵: FormationSpawnEventはApplication層の
  // イベント種別のため、エンジン組み込みのSpawnEventとは別にここで実行時登録する）
  AbsoluteEngine::TimelineEventFactory::GetInstance().Register("FormationSpawnEvent", []() {
      return std::make_unique<FormationSpawnEvent>();
  });

  // オートロード：保存されたシーンを読み込む
  LoadEditorScene();

  // -----------------------------------------------------------------------
  // 【バグ修正】エディタで配置した敵への onDestroyed コールバック一括登録
  // SpawnManager 経由で生成した敵は Spawn 時にコールバックを登録するが、
  // JSON/エディタからロードした敵にはその処理が走らないため、ここで補完する。
  // -----------------------------------------------------------------------
  for (const auto& obj : rootObjects_) {
      RegisterEnemyCallbacks(obj);
  }

  // シーン開始直後の初期視点がワープしないように1度更新して位置を確定させる
  GetMainCamera()->Update(*services_.input);

  // プレイヤーを探す（ロードされたデータにあるか）
  playerObj_ = FindPlayerObject_();

  // プレイヤーがいなければ生成
  if (!playerObj_) {
      playerObj_ = std::make_shared<AbsoluteEngine::GameObject>("Player");
      auto playerModelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
      playerModelComp->LoadModel("resources/app/player/player.obj");
      playerObj_->AddComponent(std::move(playerModelComp));
      rootObjects_.push_back(playerObj_);
  }
  
  // PlayerComponent がアタッチされているか確認し、無ければ追加・初期化
  auto playerComp = playerObj_->GetComponent<PlayerComponent>();
  if (!playerComp) {
      auto pComp = std::make_unique<PlayerComponent>();
      pComp->Initialize();
      playerObj_->AddComponent(std::move(pComp));
  } else {
      playerComp->Initialize();
  }

  // -----------------------------------------------------------------------
  // プレイヤーモデルを骨あり(Skinning対応)のgltfに強制的に差し替える。
  // resources/editor/scenes/Game.json 上は旧player.objのままでも、
  // 実行時に必ずSkinning/Animation機能が可視化されるようにするための処置。
  // -----------------------------------------------------------------------
  {
      auto* playerModelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>();
      if (!playerModelComp) {
          auto newModelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
          playerObj_->AddComponent(std::move(newModelComp));
          playerModelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>();
      }
      playerModelComp->LoadModel("resources/app/human/walk.gltf");

      if (!playerWalkAnim_) {
          playerWalkAnim_ = AnimationManager::GetInstance()->LoadAnimation("resources/app/human", "walk.gltf");
      }
      // ロックオンON/OFF時のAnimation補間（クロスフェード）実演用に別アニメーションもロードしておく
      if (!playerSneakWalkAnim_) {
          playerSneakWalkAnim_ = AnimationManager::GetInstance()->LoadAnimation("resources/app/human", "sneakWalk.gltf");
      }
      if (auto* instance = playerModelComp->GetModelInstance()) {
          instance->PlayAnimation(playerWalkAnim_, true);
      }
  }

  // -----------------------------------------------------------------------
  // GPU Particle拡張の常時可視化（加点要素）：
  // プレイヤー付近に常駐エミッタ(Box)を1つ設置し、Vortex Fieldで渦を巻かせる。
  // フレーム開始直後からゲーム画面で複数エミッタ/Fieldが確認できるようにする。
  // -----------------------------------------------------------------------
  {
      GPUParticleManager::EmitterDesc desc;
      desc.shape = GPUParticleManager::EmitterShape::Box;
      desc.translate = { 0.0f, 2.0f, 10.0f };
      desc.halfExtents = { 2.0f, 2.0f, 2.0f };
      desc.count = 12;
      desc.frequency = 0.3f;
      GPUParticleManager::GetInstance()->CreateEmitter(desc);

      GPUParticleManager::FieldDesc fieldDesc;
      fieldDesc.type = GPUParticleManager::FieldType::Vortex;
      fieldDesc.target = { 0.0f, 2.0f, 10.0f };
      fieldDesc.direction = { 0.0f, 1.0f, 0.0f };
      fieldDesc.strength = 1.5f;
      GPUParticleManager::GetInstance()->SetField(0, fieldDesc);
  }

  Renderer::GetInstance()->InitializePostProcess(1280, 720);

  // デフォルトのライトを探す
  bool hasLight = false;
  for (const auto& obj : rootObjects_) {
      if (obj && obj->GetComponent<AbsoluteEngine::LightNodeComponent>()) {
          hasLight = true;
          break;
      }
  }
  if (!hasLight) {
      auto initialDirLight = std::make_shared<AbsoluteEngine::GameObject>("Directional Light");
      initialDirLight->GetTransform().rotate = { 0.5f, 0.5f, 0.0f };
      auto lightComp = std::make_unique<AbsoluteEngine::LightNodeComponent>();
      lightComp->type = AbsoluteEngine::LightNodeComponent::Type::Directional;
      lightComp->color = { 1.0f, 1.0f, 1.0f };
      lightComp->intensity = 1.0f;
      initialDirLight->AddComponent(std::move(lightComp));
      rootObjects_.push_back(initialDirLight);
  }

  // レールカメラを探す
  bool hasRailCamera = false;
  for (const auto& obj : rootObjects_) {
      if (obj && obj->GetComponent<RailCameraComponent>()) {
          hasRailCamera = true;
          break;
      }
  }
  if (!hasRailCamera) {
      auto railCamObj = std::make_shared<AbsoluteEngine::GameObject>("RailCamera");
      auto rComp = std::make_unique<RailCameraComponent>();
      railCamObj->AddComponent(std::move(rComp));
      rootObjects_.push_back(railCamObj);
  }

  // -----------------------------------------------------------------------
  // MultiMesh & MultiMaterial対応（加点要素）の常時可視化：
  // multiMaterial.objは複数メッシュ×複数マテリアルを持つアセットで、
  // Renderer::DrawModelのサブメッシュ描画パスをゲームシーン上で確認できるようにする。
  // -----------------------------------------------------------------------
  {
      // シーンJSONへのオートセーブで既に保存されている場合に毎回重複生成しないよう、
      // 名前で既存チェックしてから生成する。
      bool hasMultiMaterialDecoration = false;
      for (const auto& obj : rootObjects_) {
          if (obj && obj->GetName() == "MultiMaterialDecoration") {
              hasMultiMaterialDecoration = true;
              break;
          }
      }
      if (!hasMultiMaterialDecoration) {
          auto multiMaterialObj = std::make_shared<AbsoluteEngine::GameObject>("MultiMaterialDecoration");
          multiMaterialObj->GetTransform().translate = { 8.0f, 1.0f, 5.0f };
          multiMaterialObj->GetTransform().scale = { 2.0f, 2.0f, 2.0f };
          auto multiMaterialModelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
          multiMaterialModelComp->LoadModel("resources/app/multiMaterial/multiMaterial.obj");
          multiMaterialObj->AddComponent(std::move(multiMaterialModelComp));
          rootObjects_.push_back(multiMaterialObj);
      }
  }

  // HUDの初期化
  gameHUD_ = std::make_unique<GameHUD>();
  gameHUD_->Initialize();

  // -----------------------------------------------------------------------
  // タイムラインマネージャーにRailCameraComponentを登録する
  // LoadEditorScene() 後に rootObjects_ を走査して RailCameraComponent を探し、
  // BaseScene 経由でタイムラインと紐付ける
  // -----------------------------------------------------------------------
  for (const auto& obj : rootObjects_) {
      if (!obj) continue;
      auto* railComp = obj->GetComponent<RailCameraComponent>();
      if (railComp) {
          SetTimelineCamera(railComp);
          break; // レールカメラは1つだけなので最初に見つかったものを使う
      }
  }
  // SpawnEventの自動配置距離: このゲームではプレイヤーはカメラから約22ユニット前方、
  // 遠レティクルはさらに約60ユニット先にあるため、その付近に出現するよう80ユニットにする
  SetTimelineSpawnAheadDistance(80.0f);
  // タイムラインデータを読み込む（ファイルが無ければ空の状態で開始）
  LoadTimeline();

#ifndef USE_IMGUI
  // ImGuiが無いビルド（Release）ではPlayボタンを押す手段が無いため、
  // タイトルからゲームシーンに来た時点で自動的にPlayモードへ入る。
  BackupScene();
  SetPlayMode(PlayMode::Play);
  timelineManager_->Play();
#endif
}

void GameScene::Finalize() {
  MouseCapture::Apply(false); // 隠していたOSマウスカーソルを必ず元に戻す
}

// -----------------------------------------------------------------------
// GetEditorViewCamera オーバーライド
// Draw() のカメラ選択分岐（isDebugCamera_ / playMode_ による切り替え）と完全に一致させる。
// ここがズレると、Viewportに表示されている絵と、ギズモ・クリック判定の座標系が
// 食い違い、「実際のゲームカメラとは関係ない視点」に見えるバグになる。
// -----------------------------------------------------------------------
Camera* GameScene::GetEditorViewCamera() const {
    if (isDebugCamera_) {
        if (playMode_ == PlayMode::Edit && editorCamera_) {
            return editorCamera_.get();
        }
        return debugCamera_.get();
    }
    return GetMainCamera();
}

// -----------------------------------------------------------------------
// AddRootObject オーバーライド（タスクC: 依存性注入）
// BaseScene::AddRootObject を呼んだ後、追加オブジェクトが EnemyComponent を持って
// いれば onDestroyed コールバックを自動注入する。
// タイムライン経由でスポーンされた敵にもコールバックが登録される。
// -----------------------------------------------------------------------
void GameScene::AddRootObject(std::shared_ptr<AbsoluteEngine::GameObject> obj) {
    // まず基底クラスの処理でリストに追加する
    BaseScene::AddRootObject(obj);

    RegisterEnemyCallbacks(obj);
}

// -----------------------------------------------------------------------
// RegisterEnemyCallbacks
// EnemyComponent::onDestroyed の登録を1箇所に集約したもの
// （Initialize でのロード済みオブジェクト・AddRootObject 経由・タイムライン生成の全てで共通）。
// FormationMemberComponent を持つ場合は編隊トラッキングの初期カウントも行う。
// -----------------------------------------------------------------------
void GameScene::RegisterEnemyCallbacks(const std::shared_ptr<AbsoluteEngine::GameObject>& obj) {
    if (!obj) return;

    auto* enemyComp = obj->GetComponent<EnemyComponent>();
    if (!enemyComp) return;

    // 既にコールバックが登録済みの場合は上書きしない（二重登録防止）
    if (enemyComp->onDestroyed) return;

    // 編隊メンバーなら初期カウントを積んでおく。
    // 生ポインタをラムダにキャプチャするが、FormationMemberComponentはEnemyComponentと
    // 同じGameObjectがunique_ptrで所有しているため、EnemyComponent（=このラムダ自身の所有者）が
    // 生きている間は必ず有効。shared_ptr<GameObject>を直接キャプチャすると
    // GameObject→EnemyComponent→(このラムダ)→GameObjectの循環参照でリークするため、
    // あえてGameObjectそのものは持たない。
    FormationMemberComponent* formationComp = obj->GetComponent<FormationMemberComponent>();
    if (formationComp) {
        scoreTracker_.RegisterFormationMember(formationComp->GetFormationId());
    }

    enemyComp->onDestroyed = [this, formationComp](const Vector3& hitPos) {
        // ヒットエフェクト（パーティクル・リングエフェクト・爆発ライト）
        hitEffects_.Spawn(*this, hitPos);

        // カメラシェイク・画面歪み（RadialBlur の中心は敵のスクリーン座標）
        postEffects_.OnEnemyDestroyed(WorldToScreenUv_(hitPos));

        // スコア加算。キルストリークに到達したら演出を発火
        if (scoreTracker_.OnEnemyKilled()) {
            postEffects_.OnKillStreak();
        }

        // 編隊トラッキング：編隊が全滅したら強化版の撃破エフェクト
        // （通常撃破時のEmitBurst(pos, 20)より大幅増量）
        if (formationComp && scoreTracker_.OnFormationMemberKilled(formationComp->GetFormationId()) > 0) {
            GPUParticleManager::GetInstance()->EmitBurst(hitPos, 60, 0.6f);
        }
    };
}

std::shared_ptr<AbsoluteEngine::GameObject> GameScene::FindPlayerObject_() const {
  for (const auto& obj : rootObjects_) {
      if (!obj) continue;
      if (obj->GetName() == "Player" || obj->GetName() == "player" || obj->GetComponent<PlayerComponent>()) {
          return obj;
      }
  }
  return nullptr;
}

Camera* GameScene::GetGameplayCamera_() const {
  return isDebugCamera_
      ? static_cast<Camera*>(debugCamera_.get())
      : static_cast<Camera*>(GetMainCamera());
}

Vector2 GameScene::WorldToScreenUv_(const Vector3& worldPos) const {
  Camera* cam = GetGameplayCamera_();
  if (!cam) return { 0.5f, 0.5f };

  const Matrix4x4 vp = Multiply(cam->GetViewMatrix(), cam->GetProjectionMatrix());
  const Vector3 ndc = TransformPoint(worldPos, vp);
  return { ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f };
}

void GameScene::UpdatePostEffects_(float deltaTime) {
  PostEffectDirector::FrameState state;
  state.isBossPhase = phase_ == GamePhase::Boss;
  if (playerObj_) {
      if (auto* pComp = playerObj_->GetComponent<PlayerComponent>()) {
          state.hasPlayer     = true;
          state.isLockingMode = pComp->lockon_.IsLockingMode();
          state.playerHp      = pComp->GetHp();
      }
  }
  postEffects_.Update(deltaTime, time_, state);
}

void GameScene::UpdateMouseCapture_() {
  // Development では ImGui のエディタを操作する必要があるため、Escでいつでも解除/再開できる
  if (services_.input && services_.input->TriggerKey(DIK_ESCAPE)) {
      mouseCaptureSuspended_ = !mouseCaptureSuspended_;
  }

  const bool isInGame = playMode_ == PlayMode::Play && !isDebugCamera_
                     && (phase_ == GamePhase::InProgress || phase_ == GamePhase::Boss);
  MouseCapture::Apply(isInGame && !mouseCaptureSuspended_);
}

void GameScene::RemoveExpiredObjects_() {
  // プレイヤーより十分後方に離れた敵を削除する。
  // 【バグ修正】敵は撃破以外で消える手段が無く、自機が通り過ぎた後もずっと
  // 生き残ってカメラ視錐台に入り続け、タレット等が背後から撃ち続けていた。
  constexpr float kEnemyDespawnBehindDistance = 15.0f;
  const Vector3 playerPos = playerObj_ ? playerObj_->GetTransform().translate : Vector3{ 0.0f, 0.0f, 0.0f };
  const Vector3 forward   = GetMainCamera()->GetForward();

  auto isExpired = [&](const std::shared_ptr<AbsoluteEngine::GameObject>& obj) {
      if (!obj) return false;

      // 寿命切れの爆発ライト
      if (auto* light = obj->GetComponent<ExplosionLightComponent>()) {
          if (light->IsDead()) return true;
      }

      if (playerObj_ && obj->GetTag() == "Enemy") {
          const Vector3& enemyPos = obj->GetTransform().translate;
          const Vector3 toEnemy = { enemyPos.x - playerPos.x, enemyPos.y - playerPos.y, enemyPos.z - playerPos.z };
          const float behindDistance = -Dot(toEnemy, forward);
          return behindDistance > kEnemyDespawnBehindDistance;
      }
      return false;
  };
  rootObjects_.erase(std::remove_if(rootObjects_.begin(), rootObjects_.end(), isExpired), rootObjects_.end());
}

void GameScene::SpawnBoss_() {
  // 【調査用の一時ログ】原因が分かったら削除する。
  Logger::GetInstance().Log("[BossPhase] start");

  // ボス出現演出（PostEffect加点要素: GaussianFilter）
  postEffects_.OnBossIntro();

  auto bossObj = std::make_shared<AbsoluteEngine::GameObject>("Boss");
  Vector3 forward = GetMainCamera()->GetForward();
  // 初期出現位置。以降はBossComponent::UpdateMovement()が毎フレーム
  // 自機のちょっと前(距離14)に追従＋左右上下の揺れを加えて上書きするため、
  // ここは最初の1フレーム分の暫定位置（BossComponent::kHoverDistanceFromPlayerと合わせておく）。
  constexpr float kBossInitialDistanceFromPlayer = 14.0f;
  Vector3 playerPos = playerObj_ ? playerObj_->GetTransform().translate : GetMainCamera()->GetEye();
  bossObj->GetTransform().translate = {
      playerPos.x + forward.x * kBossInitialDistanceFromPlayer,
      playerPos.y + forward.y * kBossInitialDistanceFromPlayer,
      playerPos.z + forward.z * kBossInitialDistanceFromPlayer
  };
  bossObj->GetTransform().scale = {4.0f, 4.0f, 4.0f};
  auto bossModelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
  bossModelComp->LoadModel("resources/app/cube/cube.obj");
  bossModelComp->SetEnvironmentCoefficient(1.0f);
  bossModelComp->SetColor({0.7f, 0.1f, 0.9f, 1.0f}); // ザコ敵と見分けられるようボスは紫に
  bossObj->AddComponent(std::move(bossModelComp));
  // 攻撃パターンはBossComponent自身が専用ロジックで管理する（汎用EnemyShootComponentには依存しない）
  bossObj->AddComponent(std::make_unique<BossComponent>());

  auto colliderComp = std::make_unique<AbsoluteEngine::ColliderComponent>();
  colliderComp->type = AbsoluteEngine::ColliderComponent::Type::Sphere;
  colliderComp->radius = 5.0f; // ボスなので大きめに
  bossObj->AddComponent(std::move(colliderComp));
  bossObj->SetTag("Boss");

  rootObjects_.push_back(bossObj);
}

void GameScene::CheckGameEnd_() {
  if (playerObj_) {
      auto pComp = playerObj_->GetComponent<PlayerComponent>();
      if (pComp && pComp->IsDead()) {
          phase_ = GamePhase::GameOver;
          RequestSceneChange(SceneId::GameOver);
      }
  }

  if (phase_ == GamePhase::Boss) {
      const bool bossAlive = std::any_of(rootObjects_.begin(), rootObjects_.end(),
          [](const auto& obj) { return obj && obj->GetComponent<BossComponent>(); });
      if (!bossAlive) {
          phase_ = GamePhase::Clear;
          RequestSceneChange(SceneId::Clear);
      }
  }
}

// Animation補間（加点要素）：ロックオンのON/OFF切り替わり時に
// 歩行アニメーション同士をクロスフェードで遷移させる。
void GameScene::UpdatePlayerAnimation_(bool isLockingMode) {
  if (isLockingMode == wasLockingMode_) return;

  if (auto* playerModelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>()) {
      if (auto* instance = playerModelComp->GetModelInstance()) {
          instance->PlayAnimation(isLockingMode ? playerSneakWalkAnim_ : playerWalkAnim_, true, 0.3f);
      }
  }
  wasLockingMode_ = isLockingMode;
}

void GameScene::UpdateHud_() {
  if (!gameHUD_) return;

  Camera* hudCam = GetGameplayCamera_();
  auto* pComp = playerObj_ ? playerObj_->GetComponent<PlayerComponent>() : nullptr;

  if (!pComp) {
      gameHUD_->Update(0, 0, {}, false, { ScreenSize::kCenterX, ScreenSize::kCenterY },
                       hudCam, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 });
      return;
  }

  const bool isLockingMode = pComp->lockon_.IsLockingMode();
  UpdatePlayerAnimation_(isLockingMode);

  gameHUD_->Update(pComp->GetHp(), pComp->GetMaxHp(),
                   pComp->lockon_.GetLockedScreenPositions(GetMainCamera()), isLockingMode,
                   pComp->GetCursorPos(), hudCam,
                   pComp->GetNearReticleWorldPos(), pComp->GetMidReticleWorldPos(), pComp->GetFarReticleWorldPos());
}

void GameScene::Update() {
  const float deltaTime = 1.0f / 60.0f;
  time_ += deltaTime;

  playerObj_ = FindPlayerObject_();

  // ポストエフェクトの調停（Editモードでもタイマーは進める）
  UpdatePostEffects_(deltaTime);

  UpdateEditor();

  UpdateMouseCapture_();

  if (playMode_ == PlayMode::Play) {
#ifdef USE_IMGUI
      bool isDragging = ImGui::GetDragDropPayload() != nullptr;
#else
      bool isDragging = false;
#endif

      if (isDebugCamera_) {
          if (!isDragging) debugCamera_->Update(*services_.input);
      } else {
          // ゲーム（レール）カメラ進行
          CameraContext ctx{};
          ctx.deltaTime = deltaTime;
          GetMainCamera()->SetContext(ctx);
          if (!isDragging && phase_ == GamePhase::InProgress) GetMainCamera()->Update(*services_.input);
      }

      // 骨のデバッグ表示トグル（Bキー）
      if (services_.input && services_.input->TriggerKey(DIK_B)) {
          showDebugSkeleton_ = !showDebugSkeleton_;
      }

      // ヒットエフェクトはイベントコールバック（onDestroyed）によって
      // EnemyComponent::Update 内から自動的に発火されるため、ここでのポーリングは不要。

      RemoveExpiredObjects_();

      // パーティクルの更新（爆発エフェクト等）
      ParticleManager::GetInstance()->Update(deltaTime);

      // EffectManagerの更新（RingEffectなど）
      EffectManager::GetInstance()->Update(deltaTime, GetGameplayCamera_());

      // ボスフェーズ移行
      float railProgress = 0.0f;
      for (const auto& obj : rootObjects_) {
          if (auto comp = obj->GetComponent<RailCameraComponent>()) {
              railProgress = comp->GetProgress();
              break;
          }
      }
      if (phase_ == GamePhase::InProgress && railProgress >= 1.0f) {
          phase_ = GamePhase::Boss;
          SpawnBoss_();
      }

      CheckGameEnd_();
      hitEffects_.Update();
      UpdateHud_();
  }

  // シーンの自動セーブ（Editモード専用。Play中に動的スポーンされた敵/弾/ボスを含む
  // 現在の rootObjects_ をそのまま保存してしまうとレベルデータが壊れるため、
  // 実プレイ中（常にPlayMode::Play）は絶対に走らせない）
  if (playMode_ == PlayMode::Edit) {
      bool shouldSave = false;
      for (const auto& obj : rootObjects_) {
          if (auto rComp = obj->GetComponent<RailCameraComponent>()) {
              if (rComp->ConsumeModifiedFlag()) {
                  shouldSave = true;
              }
          }
      }
      if (shouldSave) {
          SaveEditorScene();
      }
  }
}

void GameScene::Draw() {
  auto *renderer = Renderer::GetInstance();
  renderer->BeginRenderScene();

  Camera* activeCamera = nullptr;
  if (isDebugCamera_) {
      // Debug Camera ON: エディット中はEditorCamera、プレイ中はDebugCameraを使用する
      if (playMode_ == PlayMode::Edit && editorCamera_) {
          renderer->SetCamera(*editorCamera_);
          activeCamera = editorCamera_.get();
      } else {
          renderer->SetCamera(*debugCamera_);
          activeCamera = debugCamera_.get();
      }
  } else {
      // Debug Camera OFF: エディット/プレイ共通で GetMainCamera() を標準描画に使用する（タスクD）
      renderer->SetCamera(*GetMainCamera());
      activeCamera = GetMainCamera();
  }

  renderer->SetEnvironmentMap(skybox_.GetTexture());
  renderer->SetRandomParam(time_);

  // --- ライトの適用 ---
  ApplyEditorLightsToRenderer(renderer);

  // オブジェクトの描画
  // 背景: 手続き生成の雲海。比較用に、従来のスカイボックスへ切り替えられる
  if (useCloudSea_) {
      renderer->DrawCloudSea(cloudSea_);
  } else {
      skybox_.Draw();
  }

  // エディタ上で配置したオブジェクト群の描画
  for (auto& obj : rootObjects_) {
      if (obj) obj->Draw();
  }

  hitEffects_.Draw(renderer);

  // EffectManager（RingEffect等）の描画
  EffectManager::GetInstance()->Draw();

  // パーティクルの描画（爆発エフェクト等）
  ParticleManager::GetInstance()->Draw(BlendMode::Add);

  // GPU Particleの描画（加点要素: GPU Particle拡張）
  Renderer::GetInstance()->DrawGPUParticles(BlendMode::Add);

  // --- デバッグラインの描画 ---
  if (showDebugRail_) {
      for (const auto& obj : rootObjects_) {
          if (auto* rComp = obj->GetComponent<RailCameraComponent>()) {
              rComp->DrawDebugLine(renderer);
              break;
          }
      }
  }

  // グリッドの描画（進行感を演出）
  renderer->DrawGrid(500.0f, 50, Vector4{0.2f, 0.4f, 0.8f, 0.5f});

  // 骨のデバッグ表示（Bキートグル）
  if (showDebugSkeleton_ && playerObj_) {
      if (auto* modelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>()) {
          if (auto* instance = modelComp->GetModelInstance()) {
              instance->DrawSkeleton();
          }
      }
  }

  renderer->RenderPrimitives();

  // HUD等の2Dスプライト描画
  if (playMode_ == PlayMode::Play && gameHUD_) {
      gameHUD_->Draw();
  }

  Matrix4x4 projInverse;
  if (isDebugCamera_) {
      // Debug Camera ON: エディット中はEditorCamera、プレイ中はDebugCameraのプロジェクション逆行列を使用
      if (playMode_ == PlayMode::Edit && editorCamera_) projInverse = Inverse(editorCamera_->GetProjectionMatrix());
      else projInverse = Inverse(debugCamera_->GetProjectionMatrix());
  } else {
      // Debug Camera OFF: GetMainCamera() のプロジェクション逆行列を使用（タスクD）
      projInverse = Inverse(GetMainCamera()->GetProjectionMatrix());
  }

  renderer->EndRenderScene(projInverse);
}



void GameScene::DrawEditorUI() {
    BaseScene::DrawEditorUI(); // ツールバー等の描画

#ifdef USE_IMGUI
    // -----------------------------------------------------------------------
    // Debug Camera トグルはTimeline Editor内に一本化した（GetDebugCameraFlag経由）
    // ここにあった専用ツールバーウィンドウは廃止
    // -----------------------------------------------------------------------

    ImGui::Begin("GameScene Controls##LeftPanel");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Cloud Sea")) {
        ImGui::Checkbox("Use Cloud Sea (off = Skybox)", &useCloudSea_);
        CloudSeaDebugUi::Draw(cloudSea_);
        ImGui::Separator();
    }
    
    int hp = 0;
    int maxHp = 0;
    if (playerObj_) {
        for (const auto& comp : playerObj_->GetComponents()) {
            if (comp->GetTypeName() == "PlayerComponent") {
                if (auto* pComp = dynamic_cast<PlayerComponent*>(comp.get())) {
                    hp = pComp->GetHp();
                    maxHp = pComp->GetMaxHp();
                }
            }
        }
    }
    ImGui::Text("Player HP: %d / %d", hp, maxHp);

    ImGui::Text("Score: %d  (Kills: %d)", scoreTracker_.GetScore(), scoreTracker_.GetKillCount());

    // ボスHP（ボス出現中のみ表示。フェーズも併記して状況が伝わるようにする）
    if (phase_ == GamePhase::Boss) {
        for (const auto& obj : rootObjects_) {
            if (!obj) continue;
            if (auto* bComp = obj->GetComponent<BossComponent>()) {
                const char* phaseName = "1";
                switch (bComp->GetPhase()) {
                    case BossPhase::Phase1: phaseName = "1"; break;
                    case BossPhase::Phase2: phaseName = "2"; break;
                    case BossPhase::Phase3: phaseName = "3"; break;
                }
                ImGui::TextColored(ImVec4(0.8f, 0.3f, 1.0f, 1.0f), "BOSS HP: %d / %d  (Phase %s)",
                    bComp->GetHp(), bComp->GetMaxHp(), phaseName);
                break;
            }
        }
    }

    ImGui::SeparatorText("Camera Controls");
    Vector3 eye = GetMainCamera()->GetEye();
    Vector3 target = GetMainCamera()->GetTarget();
    ImGui::Text("Camera Eye: (%.2f, %.2f, %.2f)", eye.x, eye.y, eye.z);
    ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)", target.x, target.y, target.z);
    
    if (showDebugRail_) {
        RailCameraComponent* rComp = nullptr;
        for (const auto& obj : rootObjects_) {
            if (auto comp = obj->GetComponent<RailCameraComponent>()) {
                rComp = comp;
                break;
            }
        }
        if (rComp) {
            ImGui::Text("Rail Progress: %.1f %%", rComp->GetProgress() * 100.0f);
        }
    }

    // Debug Cameraのトグルボタン自体はTimeline Editorに一本化したため、ここでは状態表示のみ行う
    if (isDebugCamera_) {
        ImGui::TextDisabled("Debug Camera: ON (Timeline Editorで切替)");
        Vector3 camPos = debugCamera_->GetTranslate();
        if (ImGui::DragFloat3("Debug Camera Pos", &camPos.x, 0.1f)) {
            debugCamera_->SetTranslate(camPos);
        }
        Vector3 camRot = debugCamera_->GetRotation();
        if (ImGui::DragFloat3("Debug Camera Rot", &camRot.x, 0.05f)) {
            debugCamera_->SetRotation(camRot);
        }
    }
    ImGui::Checkbox("Show Rail Debug Line", &showDebugRail_);
    if (ImGui::Button("Reset Rail Camera")) {
        for (const auto& obj : rootObjects_) {
            if (auto rComp = obj->GetComponent<RailCameraComponent>()) {
                rComp->ResetProgress();
                break;
            }
        }
        CameraContext ctx{};
        ctx.deltaTime = 1.0f / 60.0f;
        GetMainCamera()->SetContext(ctx);
        if (services_.input) GetMainCamera()->Update(*services_.input);
    }

    ImGui::End();
#endif
}