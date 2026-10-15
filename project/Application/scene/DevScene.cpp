#define NOMINMAX
#include "DevScene.h"
#include "SceneIds.h"
#include "SceneManager.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AbsoluteEngine/scene/DissolveComponent.h"
#include "AbsoluteEngine/scene/LightNodeComponent.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "AbsoluteEngine/scene/CollisionManager.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "../actor/Bullet/BulletComponent.h"
#include "Enemy/EnemyComponent.h"
#include "AbsoluteEngine/editor/Command.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "graphics/Renderer.h"
#include "AbsoluteEngine/editor/EditorCamera.h"
#include "AbsoluteEngine/editor/EditorUIManager.h"
#include "AbsoluteEngine/base/FrameWork.h"
#include "AbsoluteEngine/base/WinApp.h"
#include "DirectXCommon.h"
#include "Renderer.h"
#include "TextureResource.h"
#include "graphics/texture/TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "graphics/particle/EffectManager.h"
#include "graphics/particle/RingEffect.h"
#include "graphics/particle/CylinderEffect.h"
#include "graphics/particle/PlaneHitEffect.h"
#include "graphics/particle/GPUParticleManager.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <random>
#include "AbsoluteEngine/scene/ComponentFactory.h"

static void FatalBoxAndTerminate_(const std::string &msg) {
  MessageBoxA(nullptr, msg.c_str(), "Fatal", MB_OK | MB_ICONERROR);
  std::terminate();
}

DevScene::DevScene() {}
DevScene::~DevScene() {}

void DevScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);

  InitResources_();



  InitCamera_();

  accelerationField_.area.max = {1.0f, 1.0f, 1.0f};

  // --- エディタUIの処理オートロード ---
  if (!LoadEditorScene()) {
      // ファイルが無い場合はデフォルトの初期配置
      auto obj1 = std::make_shared<AbsoluteEngine::GameObject>("Player");
      auto modelComp1 = std::make_unique<AbsoluteEngine::ModelComponent>();
      modelComp1->LoadModel("resources/app/cube/cube.obj");
      obj1->AddComponent(std::move(modelComp1));
      obj1->GetTransform().translate = { 0.0f, -1.1f, -15.0f };
      rootObjects_.push_back(obj1);
      
      // 初期状態を保存しておく
      SaveEditorScene();
  }

#ifndef USE_IMGUI
  playMode_ = PlayMode::Play;
#endif
}

void DevScene::Finalize() {
  if (logStream_.is_open()) {
    logStream_.flush();
    logStream_.close();
  }
}

void DevScene::Update() {
  const float deltaTime = 1.0f / 60.0f;
  time_ += deltaTime;

  // カメラシェイク更新
  if (cameraShakeTimer_ < cameraShakeDuration_) {
      cameraShakeTimer_ += deltaTime;
      float t = 1.0f - (cameraShakeTimer_ / cameraShakeDuration_);
      float intensity = cameraShakeIntensity_ * t;
      float rX = ((rand() % 100) / 100.0f * 2.0f - 1.0f) * intensity;
      float rY = ((rand() % 100) / 100.0f * 2.0f - 1.0f) * intensity;
      float rZ = ((rand() % 100) / 100.0f * 2.0f - 1.0f) * intensity;
      cameraShakeOffset_ = {rX, rY, rZ};
  } else {
      cameraShakeOffset_ = {0.0f, 0.0f, 0.0f};
  }

  // 画面歪み更新
  if (hitDistortionTimer_ < hitDistortionDuration_) {
      hitDistortionTimer_ += deltaTime;
      float t = 1.0f - (hitDistortionTimer_ / hitDistortionDuration_);
      Renderer::GetInstance()->SetRadialBlurParam(radialBlurCenter_, hitDistortionIntensity_ * t);
      Renderer::GetInstance()->SetPostProcessMode(Renderer::PostProcessMode::RadialBlur);
  } else if (hitDistortionDuration_ > 0.0f) {
      Renderer::GetInstance()->SetRadialBlurParam(radialBlurCenter_, 0.0f);
      Renderer::GetInstance()->SetPostProcessMode(Renderer::PostProcessMode::Normal);
      hitDistortionDuration_ = 0.0f;
  }

  // --- エネミースポーン処理 ---
  if (playMode_ == PlayMode::Play) {
      int enemyCount = 0;
      for (const auto& obj : rootObjects_) {
          if (obj && obj->GetName() == "Enemy") {
              enemyCount++;
          }
      }
      
      if (enemyCount < maxEnemies_) {
          enemySpawnTimer_ += deltaTime;
          if (enemySpawnTimer_ >= enemySpawnInterval_) {
              enemySpawnTimer_ = 0.0f;
              // ランダムな次回スポーン間隔（1.0〜3.0秒）
              enemySpawnInterval_ = 1.0f + static_cast<float>(rand() % 200) / 100.0f;

              auto newEnemy = std::make_shared<AbsoluteEngine::GameObject>("Enemy");
              newEnemy->AddComponent(std::make_unique<AbsoluteEngine::DissolveComponent>());
              auto modelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
              modelComp->LoadModel("resources/app/sphere/sphere.obj");
              modelComp->LoadTexture("resources/app/cube/white100x100.png");
              newEnemy->AddComponent(std::move(modelComp));
              
              // ランダムな出現位置 (X: -15〜15, Y: -5〜5, Z: 5〜25)
              float rX = ((rand() % 300) / 10.0f) - 15.0f;
              float rY = ((rand() % 100) / 10.0f) - 5.0f;
              float rZ = ((rand() % 200) / 10.0f) + 5.0f;
              newEnemy->GetTransform().translate = { rX, rY, rZ };
              newEnemy->SetTag("Enemy");
                
              auto colliderComp = std::make_unique<AbsoluteEngine::ColliderComponent>();
              colliderComp->type = AbsoluteEngine::ColliderComponent::Type::Sphere;
              colliderComp->radius = 2.0f;
              newEnemy->AddComponent(std::move(colliderComp));
              
              newEnemy->AddComponent(std::make_unique<EnemyComponent>());
              rootObjects_.push_back(newEnemy);
          }
      }
  }

  std::vector<std::shared_ptr<AbsoluteEngine::GameObject>> newObjects;
  
  if (services_.input) {
      float moveX = 0.0f;
      float moveY = 0.0f;

      // キーボード (矢印キーのみ)
      if (services_.input->PressKey(DIK_UP)) {
          moveY -= 1.0f;
      }
      if (services_.input->PressKey(DIK_DOWN)) {
          moveY += 1.0f;
      }
      if (services_.input->PressKey(DIK_LEFT)) {
          moveX -= 1.0f;
      }
      if (services_.input->PressKey(DIK_RIGHT)) {
          moveX += 1.0f;
      }

      // ゲームパッド対応 (左スティック)
      if (services_.input->IsGamepadConnected()) {
          auto pad = services_.input->GetGamepad();
          float padX = pad.lx / 32767.0f;
          float padY = -pad.ly / 32767.0f; // Y軸は上がプラスなので反転
          if (std::abs(padX) > 0.2f) moveX = padX;
          if (std::abs(padY) > 0.2f) moveY = padY;
      }

      // 速度の正規化
      float length = std::sqrt(moveX * moveX + moveY * moveY);
      if (length > 1.0f) {
          moveX /= length;
          moveY /= length;
      }

      reticlePos_.x += moveX * reticleSpeed_ * deltaTime;
      reticlePos_.y += moveY * reticleSpeed_ * deltaTime;

      // 画面内にクランプ (仮で1280x720)
      reticlePos_.x = std::clamp(reticlePos_.x, 0.0f, 1280.0f);
      reticlePos_.y = std::clamp(reticlePos_.y, 0.0f, 720.0f);

      // --- スナップエイムと即着弾(ヒットスキャン)の実装 ---
      if (editorCamera_) {
          Matrix4x4 viewProj = Multiply(editorCamera_->GetViewMatrix(), editorCamera_->GetProjectionMatrix());
          
          Vector3 bestEnemyPos = {0,0,0};
          bool enemyFound = false;
          bool isSnapped = false;
          float minDistSq = 150.0f * 150.0f; // スナップ判定半径の二乗
          Vector2 bestEnemyScreen;

          for(const auto& obj : rootObjects_) {
              if (obj && obj->GetName() == "Enemy") {
                  bool isDead = false;
                  for (const auto& comp : obj->GetComponents()) {
                      if (comp->GetTypeName() == "EnemyComponent") {
                          isDead = static_cast<EnemyComponent*>(comp.get())->IsDead();
                          break;
                      }
                  }
                  if (isDead) continue;

                  Vector3 ePos = obj->GetTransform().translate;
                  Vector3 ndc = TransformPoint(ePos, viewProj);
                  if (ndc.z > 0.0f && ndc.z < 1.0f) {
                      Vector2 eScreen;
                      eScreen.x = (ndc.x + 1.0f) * 0.5f * 1280.0f;
                      eScreen.y = (1.0f - ndc.y) * 0.5f * 720.0f;

                      float dx = reticlePos_.x - eScreen.x;
                      float dy = reticlePos_.y - eScreen.y;
                      float distSq = dx * dx + dy * dy;

                      if (distSq < minDistSq) {
                          minDistSq = distSq;
                          bestEnemyPos = ePos;
                          bestEnemyScreen = eScreen;
                          enemyFound = true;
                      }
                  }
              }
          }

          if (enemyFound) {
              float snapRadius = 150.0f;
              float dist = std::sqrt(minDistSq);
              isSnapped = true;
              
              float dx = reticlePos_.x - bestEnemyScreen.x;
              float dy = reticlePos_.y - bestEnemyScreen.y;

              if (dist > 1.0f) {
                  float dirX = -dx / dist;
                  float dirY = -dy / dist;
                  float pullFactor = 1.0f - (dist / snapRadius) * 0.5f; 
                  reticlePos_.x += dirX * snapPullSpeed_ * pullFactor * deltaTime;
                  reticlePos_.y += dirY * snapPullSpeed_ * pullFactor * deltaTime;
              }
          }

          // --- 発射入力判定と弾の生成 ---
          fireTimer_ += deltaTime;
          bool firePressed = services_.input->PressKey(DIK_SPACE);
          if (services_.input->IsGamepadConnected()) {
              if (services_.input->IsPadDown(XINPUT_GAMEPAD_A) || services_.input->IsPadDown(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
                  firePressed = true;
              }
          }

          if (firePressed && fireTimer_ >= fireInterval_) {
              fireTimer_ = 0.0f;
              Vector3 targetPos = {0,0,0};
              if (isSnapped) {
                  targetPos = bestEnemyPos;
              } else {
                  // レティクル方向へ飛ばす（ScreenPointToRay相当）
                  Matrix4x4 viewMat = editorCamera_->GetViewMatrix();
                  Matrix4x4 projMat = editorCamera_->GetProjectionMatrix();
                  float ndcX = (reticlePos_.x / 1280.0f) * 2.0f - 1.0f;
                  float ndcY = 1.0f - (reticlePos_.y / 720.0f) * 2.0f;
                  Matrix4x4 invViewProj = Inverse(Multiply(viewMat, projMat));
                  
                  // FarClip面上の点を計算
                  Vector3 rayFar = TransformPoint({ndcX, ndcY, 1.0f}, invViewProj);
                  
                  // カメラ位置の取得（viewMatの逆行列の平行移動成分）
                  Matrix4x4 invView = Inverse(viewMat);
                  Vector3 rayOrigin = { invView.m[3][0], invView.m[3][1], invView.m[3][2] };
                  
                  Vector3 dir = { rayFar.x - rayOrigin.x, rayFar.y - rayOrigin.y, rayFar.z - rayOrigin.z };
                  float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                  dir = { dir.x/len, dir.y/len, dir.z/len };
                  
                  targetPos = { rayOrigin.x + dir.x * 100.0f, rayOrigin.y + dir.y * 100.0f, rayOrigin.z + dir.z * 100.0f };
              }

              // 発射位置（Playerの位置）
              Vector3 spawnPos = {0,0,0};
              for(const auto& obj : rootObjects_) {
                  if (obj && obj->GetName() == "Player") {
                      spawnPos = obj->GetTransform().translate;
                      break;
                  }
              }

              // 弾の生成
              auto bullet = std::make_shared<AbsoluteEngine::GameObject>("Bullet");
              auto modelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
              modelComp->LoadModel("resources/app/sphere/sphere.obj");
              bullet->AddComponent(std::move(modelComp));
              bullet->GetTransform().translate = spawnPos;
              bullet->GetTransform().scale = {0.2f, 0.2f, 0.2f};

              // 速度計算
              Vector3 diff = { targetPos.x - spawnPos.x, targetPos.y - spawnPos.y, targetPos.z - spawnPos.z };
              float dLen = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
              float speed = 50.0f; // 弾速
              Vector3 velocity = { (diff.x/dLen)*speed, (diff.y/dLen)*speed, (diff.z/dLen)*speed };

              auto comp = std::make_unique<BulletComponent>();
              comp->Initialize(velocity);
              bullet->AddComponent(std::move(comp));

              rootObjects_.push_back(bullet);
          }
      }

      reticleSprite_.SetPosition({reticlePos_.x - reticleSize_.x / 2.0f, reticlePos_.y - reticleSize_.y / 2.0f, 0.0f});
  }

  // BaseSceneのエディタ機能（カメラ、オブジェクトの更新）
  UpdateEditor();

#ifdef USE_IMGUI
  // --- 左パネル：ライト設定（UIから削除し、インスペクター側で管理） ---

  // --- 右パネル：エフェクトテスト ---
  ImGui::Begin("Effect Test##RightPanel");
  ImGui::Text("Click to spawn effects:");
  
  if (ImGui::Button("Spawn Ring Effect")) {
      EffectManager::GetInstance()->AddEffect(std::make_unique<RingEffect>(texRing_, transform_.translate));
  }

  if (ImGui::Button("Spawn Cylinder Effect")) {
      EffectManager::GetInstance()->AddEffect(std::make_unique<CylinderEffect>(texCylinder_, transform_.translate));
  }
  
  if (ImGui::Button("Spawn Plane Hit Effect")) {
      EffectManager::GetInstance()->AddEffect(std::make_unique<PlaneHitEffect>(resEffect_, transform_.translate));
  }

  ImGui::SeparatorText("Score Features (Skinning/GPU Particle/MultiMaterial)");

  if (ImGui::Button("Toggle Animation Blend (Walk / Sneak)")) {
      isWalkAnim_ = !isWalkAnim_;
      modelHuman_.PlayAnimation(isWalkAnim_ ? animHuman_ : animHumanAlt_, true, 0.3f);
  }

  if (ImGui::Button("Fire Muzzle Flash")) {
      Matrix4x4 handWorld = RemoveScale(modelHuman_.GetBoneWorldMatrix("mixamorig:RightHand"));
      Vector3 handPos = { handWorld.m[3][0], handWorld.m[3][1], handWorld.m[3][2] };
      static std::mt19937 muzzleRng(std::random_device{}());
      std::uniform_real_distribution<float> muzzleDist(-1.0f, 1.0f);
      for (int i = 0; i < 6; ++i) {
          Vector3 vel = {
              muzzleDist(muzzleRng) * 3.0f,
              muzzleDist(muzzleRng) * 3.0f,
              muzzleDist(muzzleRng) * 3.0f
          };
          ParticleManager::GetInstance()->Emit(
              "default", handPos, vel,
              Vector3{ 0.9f, 0.9f, 0.9f }, Vector3{ 0, 0, 0 }, 0.25f,
              Vector4{ 1.0f, 0.9f, 0.4f, 1.0f });
      }
  }

  if (ImGui::Button("Spawn GPU Burst")) {
      GPUParticleManager::GetInstance()->EmitBurst(transform_.translate, 20);
  }

  ImGui::End();

  // --- 下パネル：オブジェクト・エフェクト設定 ---
  ImGui::Begin("Objects##BottomPanel");
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::Separator();
  ImGui::Checkbox("Show Skeleton", &showSkeleton_);
  ImGui::Checkbox("Enable Reflection", &enableReflection_);
  if (enableReflection_) {
    ImGui::SliderFloat("Reflection Weight", &reflectionWeight_, 0.0f, 1.0f);
  }

  const char *blendModeItems[] = {"Alpha", "Add", "Subtract", "Multiply", "Screen"};
  ImGui::Combo("Particle Blend", &particleBlendMode_, blendModeItems, IM_ARRAYSIZE(blendModeItems));

  ImGui::SeparatorText("Transform");
  ImGui::DragFloat3("Sphere Pos", &transform_.translate.x, 0.1f);
  ImGui::DragFloat3("Human Pos", &transformHuman_.translate.x, 0.1f);
  auto* editorCamera = dynamic_cast<AbsoluteEngine::EditorCamera*>(editorCamera_.get());
  if (editorCamera) {
      Vector3 camPos = editorCamera->GetTranslate();
      if (ImGui::DragFloat3("Camera Pos", &camPos.x, 0.1f)) {
          editorCamera->SetTranslate(camPos);
      }
      Vector3 camRot = editorCamera->GetRotation();
      if (ImGui::DragFloat3("Camera Rot(Pitch,Yaw,Roll)", &camRot.x, 0.05f)) {
          editorCamera->SetRotation(camRot);
      }
  } else {
      ImGui::DragFloat3("Camera Pos (Not Linked)", &cameraTransform_.translate.x, 0.1f);
  }
  ImGui::End();


  Renderer::GetInstance()->SetRandomParam(time_);

  // --- ツールバー・エディタUIの描画はGameApp側のSceneManager::DrawEditorUI()で行うため削除 ---

#endif

  // --- 弾の寿命チェックと敵の当たり判定 ---
  for (auto it = rootObjects_.begin(); it != rootObjects_.end(); ) {
      auto& obj = *it;
      if (obj && obj->GetName() == "Bullet") {
          // 寿命(Active状態)の確認
          bool isActive = true;
          for (const auto& comp : obj->GetComponents()) {
              if (comp->GetTypeName() == "BulletComponent") {
                  auto bulletComp = static_cast<BulletComponent*>(comp.get());
                  if (!bulletComp->IsActive()) {
                      isActive = false;
                  }
                  break;
              }
          }

          if (!isActive) {
              it = rootObjects_.erase(it);
              continue;
          }

          // すべての生きた敵と当たり判定
          bool hit = false;
          Vector3 bPos = obj->GetTransform().translate;
          float hitRadius = 1.5f;

          for (const auto& eObj : rootObjects_) {
              if (eObj && eObj->GetName() == "Enemy") {
                  bool isDead = false;
                  for (const auto& comp : eObj->GetComponents()) {
                      if (comp->GetTypeName() == "EnemyComponent") {
                          isDead = static_cast<EnemyComponent*>(comp.get())->IsDead();
                          break;
                      }
                  }
                  if (isDead) continue;

                  Vector3 targetEnemyPos = eObj->GetTransform().translate;
                  float dx = bPos.x - targetEnemyPos.x;
                  float dy = bPos.y - targetEnemyPos.y;
                  float dz = bPos.z - targetEnemyPos.z;
                  float distSq = dx*dx + dy*dy + dz*dz;

                  if (distSq < hitRadius * hitRadius) {
                      hit = true;
                      
                      // ヒットエフェクトの発生
                      SpawnHitEffect(targetEnemyPos);
                      EffectManager::GetInstance()->AddEffect(std::make_unique<RingEffect>(texRing_, targetEnemyPos));

                      // カメラシェイク開始
                      cameraShakeDuration_ = 0.3f;
                      cameraShakeTimer_ = 0.0f;
                      cameraShakeIntensity_ = 0.2f;

                      // ヒット時の歪み開始
                      hitDistortionDuration_ = 0.2f;
                      hitDistortionTimer_ = 0.0f;
                      hitDistortionIntensity_ = 0.05f;
                      
                      Matrix4x4 viewMat = editorCamera_->GetViewMatrix();
                      Matrix4x4 projMat = editorCamera_->GetProjectionMatrix();
                      Matrix4x4 viewProj = Multiply(viewMat, projMat);
                      Vector3 screenPos = TransformPoint(targetEnemyPos, viewProj);
                      radialBlurCenter_ = { screenPos.x * 0.5f + 0.5f, -screenPos.y * 0.5f + 0.5f };

                      // 爆発の光（ポイントライト）の生成
                      auto lightObj = std::make_shared<AbsoluteEngine::GameObject>("ExplosionLight");
                      lightObj->GetTransform().translate = targetEnemyPos;
                      auto lightNodeComp = std::make_unique<AbsoluteEngine::LightNodeComponent>();
                      lightNodeComp->type = AbsoluteEngine::LightNodeComponent::Type::Point;
                      lightNodeComp->color = { 1.0f, 0.5f, 0.1f };
                      lightNodeComp->radius = 30.0f;
                      lightNodeComp->decay = 1.0f;
                      lightObj->AddComponent(std::move(lightNodeComp));
                      auto lightComp = std::make_unique<ExplosionLightComponent>();
                      lightComp->maxIntensity_ = 20.0f;
                      lightComp->lifeTime_ = 0.5f;
                      lightObj->AddComponent(std::move(lightComp));
                      newObjects.push_back(lightObj);

                      // 敵をディゾルブ消滅させる
                      for (const auto& comp : eObj->GetComponents()) {
                          if (comp->GetTypeName() == "EnemyComponent") {
                              static_cast<EnemyComponent*>(comp.get())->OnHit();
                              break;
                          }
                      }
                      
                      break; // 1つの弾は1体の敵にしか当たらない
                  }
              }
          }

          if (hit) {
              it = rootObjects_.erase(it);
              continue;
          }
      }
      ++it;
  }

  // ディゾルブ完了のEnemyや寿命切れのライト削除
  for (auto it = rootObjects_.begin(); it != rootObjects_.end(); ) {
      auto& obj = *it;
      bool shouldDelete = false;

      if (obj) {
          if (obj->GetName() == "Enemy") {
              auto dissolveComp = obj->GetComponent<AbsoluteEngine::DissolveComponent>();
              if (dissolveComp && dissolveComp->enable && dissolveComp->threshold >= 1.0f) {
                  shouldDelete = true;
              }
          } else if (obj->GetName() == "ExplosionLight") {
              for (const auto& comp : obj->GetComponents()) {
                  if (comp->GetTypeName() == "ExplosionLightComponent") {
                      auto lComp = static_cast<ExplosionLightComponent*>(comp.get());
                      if (lComp->IsDead()) {
                          shouldDelete = true;
                      }
                      break;
                  }
              }
          }
      }

      if (shouldDelete) {
          it = rootObjects_.erase(it);
      } else {
          ++it;
      }
  }

  // 新規オブジェクトの追加
  rootObjects_.insert(rootObjects_.end(), newObjects.begin(), newObjects.end());

  EffectManager::GetInstance()->Update(deltaTime, editorCamera_.get());

  // ゲームロジックは PlayMode の時のみ更新する
  if (playMode_ == PlayMode::Play) {


      // particleEmitter_.Update(deltaTime);
      ParticleManager::GetInstance()->SetEnableAccelerationField(enableAccelerationField_);
      ParticleManager::GetInstance()->SetAccelerationField(accelerationField_);
      ParticleManager::GetInstance()->Update(deltaTime);

      modelAnimCube_.UpdateAnimation(deltaTime);
      modelSimpleSkin_.UpdateAnimation(deltaTime);
      modelHuman_.UpdateAnimation(deltaTime);

      // --- 加点要素デモ: 武器(cube)を人型モデルの右手ボーンに追従させる ---
      // GetBoneWorldMatrixが返す行列にはリグ由来のスケールが含まれることがあるため、
      // RemoveScaleで平行移動・回転のみを取り出してから武器専用のオフセットを合成する。
      {
          Matrix4x4 handWorld = RemoveScale(modelHuman_.GetBoneWorldMatrix("mixamorig:RightHand"));
          Matrix4x4 offset = MakeAffineMatrix({ 0.15f, 0.15f, 0.6f }, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.3f });
          weaponModel_.SetWorld(Multiply(offset, handWorld));
      }
  }
}

void DevScene::Draw() {
  auto* renderer = Renderer::GetInstance();
  renderer->BeginRenderScene();

  // --- カメラ・ライト設定（オフスクリーンパス前に確定させる） ---
  if (editorCamera_) {
    if (auto edCam = dynamic_cast<AbsoluteEngine::EditorCamera*>(editorCamera_.get())) {
        edCam->SetShakeOffset(cameraShakeOffset_);
        edCam->UpdateMatrix();
    }
    renderer->SetCamera(*editorCamera_);
  }
  renderer->SetEnvironmentMap(skybox_.GetTexture());
  // --- ライトの適用 ---
  ApplyEditorLightsToRenderer(renderer);

  // --- エディタ上で配置したオブジェクト群の描画 ---
  for (auto& obj : rootObjects_) {
    obj->Draw();
  }

  // --- 加点要素デモ: Skinningモデル(人型・アニメーション再生中)の描画 ---
  modelHuman_.SetWorld(MakeAffineMatrix(transformHuman_.scale, transformHuman_.rotate, transformHuman_.translate));
  modelHuman_.Draw();

  // 骨のデバッグ表示（Show Skeletonチェックボックス）
  if (showSkeleton_) {
      modelHuman_.DrawSkeleton();
  }

  // --- 加点要素デモ: 武器(cube)の手追従 ---
  weaponModel_.Draw();

  // --- 加点要素デモ: MultiMesh & MultiMaterial ---
  modelMultiMaterial_.SetWorld(MakeAffineMatrix(transformMultiMaterial_.scale, transformMultiMaterial_.rotate, transformMultiMaterial_.translate));
  modelMultiMaterial_.Draw();

  renderer->RenderPrimitives();
  skybox_.Draw();

  // 新しいエフェクトマネージャによる描画
  EffectManager::GetInstance()->Draw();

  // パーティクルの描画
  ParticleManager::GetInstance()->Draw(static_cast<BlendMode>(particleBlendMode_ + 1));

  // --- 加点要素デモ: GPU Particle拡張の描画 ---
  Renderer::GetInstance()->DrawGPUParticles(BlendMode::Add);

  // --- レティクルの描画 ---
  reticleSprite_.Draw();

  // --- ポストプロセスの適用 ---
  Matrix4x4 projInverse;
  if (editorCamera_) projInverse = Inverse(editorCamera_->GetProjectionMatrix());
  renderer->EndRenderScene(projInverse);
}

void DevScene::InitLogging_() {
  // 古いログ機能は削除されました。今後はLoggerクラスを使用します。
}

void DevScene::InitResources_() {
  auto *am = AbsoluteEngine::AssetManager::GetInstance();
  auto *dx = Renderer::GetInstance()->GetDX();

  resSphere_ = am->Load<ModelResource>("resources/app/sphere/sphere.obj");
  resTerrain_ = am->Load<ModelResource>("resources/app/terrain/terrain.obj");
  resCube_ = am->Load<ModelResource>("resources/app/cube/cube.obj");
  resAnimCube_ = am->Load<ModelResource>("resources/app/AnimatedCube/AnimatedCube.gltf");
  animCubeAnim_ = AnimationManager::GetInstance()->LoadAnimation("resources/app/AnimatedCube", "AnimatedCube.gltf");
  resEffect_ = am->Load<ModelResource>("resources/app/particle/particle.obj");

  modelSphere_.Initialize({resSphere_, {1, 1, 1, 1}, 0});
    modelTerrain_.Initialize({resTerrain_, {1, 1, 1, 1}, 0});
  modelAnimCube_.Initialize({resAnimCube_, {1, 1, 1, 1}, 1});
  if (animCubeAnim_) modelAnimCube_.PlayAnimation(animCubeAnim_, true);

  resSimpleSkin_ = am->Load<ModelResource>("resources/app/simpleSkin/simpleSkin.gltf");
  animSimpleSkin_ = AnimationManager::GetInstance()->LoadAnimation("resources/app/simpleSkin", "simpleSkin.gltf");
  modelSimpleSkin_.Initialize({resSimpleSkin_, {1, 1, 1, 1}, 1});
  if (animSimpleSkin_) modelSimpleSkin_.PlayAnimation(animSimpleSkin_, true);

  resHuman_ = am->Load<ModelResource>("resources/app/human/walk.gltf");
  animHuman_ = AnimationManager::GetInstance()->LoadAnimation("resources/app/human", "walk.gltf");
  modelHuman_.Initialize({resHuman_, {1, 1, 1, 1}, 1});
  if (animHuman_) modelHuman_.PlayAnimation(animHuman_, true);
  // Animation補間（クロスフェード）実演用：歩行⇔忍び足を切り替えるための別アニメーション
  animHumanAlt_ = AnimationManager::GetInstance()->LoadAnimation("resources/app/human", "sneakWalk.gltf");

  // --- 加点要素デモ: 武器(cube)の手追従（resCube_を流用） ---
  weaponModel_.Initialize({resCube_, {0.6f, 0.6f, 0.65f, 1.0f}, 0});

  // --- 加点要素デモ: MultiMesh & MultiMaterial ---
  resMultiMaterial_ = am->Load<ModelResource>("resources/app/multiMaterial/multiMaterial.obj");
  modelMultiMaterial_.Initialize({resMultiMaterial_, {1, 1, 1, 1}, 1});
  transformMultiMaterial_.translate = {10.0f, 0.0f, 0.0f};
  transformMultiMaterial_.scale = {2.0f, 2.0f, 2.0f};

  // --- 加点要素デモ: GPU Particle拡張（常駐Box型エミッタ + Vortex Field） ---
  {
      GPUParticleManager::EmitterDesc desc;
      desc.shape = GPUParticleManager::EmitterShape::Box;
      desc.translate = {-6.0f, 2.0f, 0.0f};
      desc.halfExtents = {2.0f, 2.0f, 2.0f};
      desc.count = 12;
      desc.frequency = 0.3f;
      GPUParticleManager::GetInstance()->CreateEmitter(desc);

      GPUParticleManager::FieldDesc fieldDesc;
      fieldDesc.type = GPUParticleManager::FieldType::Vortex;
      fieldDesc.target = {-6.0f, 2.0f, 0.0f};
      fieldDesc.direction = {0.0f, 1.0f, 0.0f};
      fieldDesc.strength = 10.0f; // GameSceneでの検証結果を踏まえ、渦がはっきり見える強さに設定
      GPUParticleManager::GetInstance()->SetField(0, fieldDesc);
  }

  sprite_.Initialize({"resources/app/plane/uvChecker.png", {640, 360}, {1, 1, 1, 1}});
  skybox_.Initialize("resources/app/dds/dds.dds");

  // --- 照準（レティクル）の初期化 ---
  texReticle_ = am->Load<TextureResource>("resources/app/Reticle/reticle.png");
  Sprite::CreateInfo reticleInfo;
  reticleInfo.texturePath = "resources/app/Reticle/reticle.png";
  reticleInfo.size = reticleSize_;
  reticleInfo.color = {1.0f, 1.0f, 1.0f, 1.0f}; // 白色
  reticleSprite_.Initialize(reticleInfo);

  texRing_ = am->Load<TextureResource>("resources/app/textures/gradationLine.png");

  texNoise0_ = am->Load<TextureResource>("resources/app/noise/noise0.png");
  Renderer::GetInstance()->SetDissolveMaskTexture(texNoise0_);

  texCylinder_ = am->Load<TextureResource>("resources/app/textures/gradationLine.png");

  ParticleManager::GetInstance()->CreateParticleGroup(particleGroupName_, "resources/app/particle/circle.png", kParticleCount_);
  ParticleManager::GetInstance()->CreateParticleGroup("explosion", "resources/app/explosion/explosion.png", 100);
  ParticleEmitter::Params p{};
  p.groupName = particleGroupName_;
  p.emitRate = 10.0f;
  particleEmitter_.Initialize(ParticleManager::GetInstance(), p);

  // --- ライトの初期設定 ---
  // 最初からシーンに配置しておくライトを rootObjects_ に追加する
  auto initialDirLight = std::make_shared<AbsoluteEngine::GameObject>("Directional Light");
  initialDirLight->GetTransform().rotate = { 0.5f, 0.5f, 0.0f }; // 適当な方向
  auto dirLightComp = std::make_unique<AbsoluteEngine::LightNodeComponent>();
  dirLightComp->type = AbsoluteEngine::LightNodeComponent::Type::Directional;
  dirLightComp->color = { 1.0f, 1.0f, 1.0f };
  dirLightComp->intensity = 1.0f;
  initialDirLight->AddComponent(std::move(dirLightComp));
  rootObjects_.push_back(initialDirLight);

  auto initialPointLight = std::make_shared<AbsoluteEngine::GameObject>("Point Light");
  initialPointLight->GetTransform().translate = { 0.0f, 2.0f, -2.0f };
  auto pointLightComp = std::make_unique<AbsoluteEngine::LightNodeComponent>();
  pointLightComp->type = AbsoluteEngine::LightNodeComponent::Type::Point;
  pointLightComp->color = { 1.0f, 1.0f, 1.0f };
  pointLightComp->intensity = 1.0f;
  pointLightComp->radius = 10.0f;
  initialPointLight->AddComponent(std::move(pointLightComp));
  rootObjects_.push_back(initialPointLight);

  // オフスクリーンテスト初期化
  Renderer::GetInstance()->InitializePostProcess(1280, 720);
}

void DevScene::InitCamera_() {
  transformTerrain_.translate = {0, -5.0f, 0};
  cameraTransform_.translate = {0, 0, -15};
  transformSimpleSkin_.translate = {3, 0, 0};
  transformHuman_.translate = {6, 0, 0};
  transformAnimCube_.translate = {-3, 0, 0};
  if (editorCamera_) editorCamera_->SetPerspective(0.45f, Renderer::GetInstance()->GetAspectRatio(), 0.1f, 1000.0f);
}

void DevScene::SpawnHitEffect(const Vector3 &pos) {
  // 爆発の画像（explosion.png）を使ったパーティクルをランダムな方向へ大きく拡散
  for (int i = 0; i < 30; ++i) {
      float rX = ((rand() % 200) / 100.0f) - 1.0f;
      float rY = ((rand() % 200) / 100.0f) - 1.0f;
      float rZ = ((rand() % 200) / 100.0f) - 1.0f;
      // 速度と大きさを中間に調整（飛び散りすぎないように）
      Vector3 vel = {rX * 1.5f, rY * 1.5f, rZ * 1.5f};
      ParticleManager::GetInstance()->Emit(
          "explosion", pos, vel,
          Vector3{1.5f, 1.5f, 1.5f}, Vector3{0, 0, 0}, 0.7f, Vector4{1, 1, 1, 1});
  }
}
