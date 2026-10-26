#define NOMINMAX
#include "GameScene.h"
#include "Renderer.h"
#include "DirectXCommon.h"
#include "DirectXResourceUtils.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "Input.h"
#include "ScopedCameraOverride.h"
#include "../camera/RailCameraComponent.h"
#include "Spline.h"
#include "Method.h"
#include "GameObject.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include "Logger.h"
#include "CpuTimer.h"
#include "GameAssetPreloader.h"

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
#include "VolumeCloudDebugUi.h"
#include "GameIntroDirector.h"
#include "StageStartDirector.h"
#include "StageNamePath.h"
#include "StageImpactEffects.h"
#include "SpeedStreaks.h"
#include "SceneManager.h"
#include "SceneIds.h"
#include "ExhibitionMode.h"
#include "StageStartDemoMode.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "AnimationManager.h"
#include "../actor/Player/PlayerModelDefs.h"
#include "AbsoluteEngine/scene/DissolveComponent.h"
#include "Input.h"
#include "AbsoluteEngine/scene/LightNodeComponent.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "graphics/particle/EffectManager.h"
#include "component/ExplosionLightComponent.h"

namespace {
// ゲームのカメラの投影(画角・ニア・ファー)
constexpr float kGameCameraFovY = 0.45f;
constexpr float kGameCameraNear = 0.1f;
constexpr float kGameCameraFar = 1000.0f;
// ステージ名の板のモデルと画像(ステージが増えたら、画像のパスをステージごとの設定へ移す)
constexpr const char *kStageNameModelPath = "resources/app/stage/stage_name_board.obj";
constexpr const char *kStageNameTexturePath = "resources/app/stage/stage_name_01.png";
// 雲の文字のパフの画像(タイトルの雲の粒と同じ)
constexpr const char *kPuffTexturePath = "resources/app/title/puff.png";
// 板が割れるときの衝撃点(板の中の座標。板の中心が (0, 0)。横が -1 〜 1、縦が -0.25 〜 0.25)。
// 自機がここを突き破る。画像の「STAGE 1」の文字の中心あたり(y は画像の上から約 1/4)
constexpr Vector2 kStageNameImpactPoint{0.0f, 0.12f};

// ステージ名の板の透明度を、ステージ開始演出の段階から決める。
// 引く間に現れ、保つ間と戻る間(割れるまで)は見えたまま。割れたあとの破片の薄れ方は、破片側が決める
// phase: 今の段階。progress: 段階の中での進み具合(0 〜 1)
float CalcStageNameAlpha(StageStartPhase phase, float progress) {
  switch (phase) {
  case StageStartPhase::PullBack:
    return progress;
  case StageStartPhase::Hold:
  case StageStartPhase::Return:
    return 1.0f;
  case StageStartPhase::Idle:
  default:
    return 0.0f;
  }
}

// 0 〜 1 を、始まりと終わりがなめらかな S 字(smoothstep)に変える(板が弧を描くときの、動き出しと着地をなめらかにする)
float SmoothStep01(float t) {
  const float c = std::clamp(t, 0.0f, 1.0f);
  return c * c * (3.0f - 2.0f * c);
}

// 速度線の板のモデル(タイトルの気流の筋と同じ)
constexpr const char *kSpeedStreakModelPath = "resources/app/title/streak.obj";
// 衝撃のフラッシュの板のモデル
constexpr const char *kImpactFlashModelPath = "resources/app/title/flash.obj";

// 板が突き破られる位置を決めるとき、「戻る」の始まりから板を割るまでに進む距離を、何個に区切って足し合わせるか
constexpr int kBreakTravelSamples = 64;

// 描画の間だけ、カメラに俯瞰のずれを重ね、スコープを抜けたら元の位置・向き・画角へ戻す(差し替えの仕組みは ScopedCameraOverride)。
// ゲームの進行(自機の位置、敵、弾)に使うカメラの位置は変えず、見せるだけのずれにする
class ScopedStageStartCameraOffset {
public:
  // camera: ずらすカメラ(nullptr ならずらさない)。pose: ずれの大きさ
  ScopedStageStartCameraOffset(GameCamera *camera, const StageStartPose &pose) : override_(MakeOverride_(camera, pose)) {}

private:
  static ScopedCameraOverride MakeOverride_(GameCamera *camera, const StageStartPose &pose) {
    // 俯瞰のずれを重ねた位置・注視点(位置が上がるだけで注視点は上げないので、見下ろす向きになり、自機の先まで見える)
    Vector3 offsetEye{0.0f, 0.0f, 0.0f};
    Vector3 offsetTarget{0.0f, 0.0f, 1.0f};
    if (camera != nullptr) {
      StageStartDirector::ApplyPose(pose, camera->GetEye(), camera->GetTarget(), offsetEye, offsetTarget);
    }
    return ScopedCameraOverride(camera, offsetEye, offsetTarget, kGameCameraFovY * pose.fovScale, kGameCameraFovY, kGameCameraNear,
                                kGameCameraFar);
  }

  ScopedCameraOverride override_;
};
// 調整 UI のスライダーの範囲(新方式の空)
constexpr float kSeaSkySunAzimuthRange = 180.0f;
constexpr float kSeaSkySunElevationMin = 5.0f;
constexpr float kSeaSkySunElevationMax = 80.0f;
constexpr float kSeaSkyFloorHeightMin = -200.0f;
constexpr float kSeaSkyFloorHeightMax = -30.0f;

// 自機のモデル(蝶。ボーン付きで、羽ばたきなどのアニメーションが入っている)と、その大きさ。翼幅(モデルの x 方向の幅)がこの値になるよう拡大する(ワールドの単位)
constexpr const char *kPlayerModelPath = GameAssetPaths::kPlayerModel;
constexpr float kPlayerWingspan = 3.7f;

// 死亡演出のスキップに使う入力(発射ボタン(Space・パッド A・マウス左)とは別のキーにして、連打による誤スキップを避ける)。
// キーボードは主列の 0(Enter は、デバッグのショートカットでクリアに使うので避ける)。パッドは Start
constexpr BYTE kDeathSkipKey = DIK_0;
constexpr WORD kDeathSkipPadButton = XINPUT_GAMEPAD_START;

// 死亡演出の間、近くの敵を描かない範囲(カメラ、蝶からの距離。ユニット)。
// 描かないだけで、敵は存在し続ける(撃破扱いにならず、スコア・編隊のカウント・撃破の演出に影響しない)
constexpr float kDeathHideEnemyRadiusFromCamera = 14.0f;
constexpr float kDeathHideEnemyRadiusFromPlayer = 9.0f;
// デバッグ用: 死亡演出を蘇生するキー(0 はスキップ。BACKSPACE で死亡)
constexpr BYTE kDebugReviveKey = DIK_9;

// 開発用のフロー確認ショートカットを有効にするか(提出・展示のビルドでは false にする)。
// SPACE はゲーム中の射撃・ロックオンに使うので、ゲーム中の確認用には使わない
constexpr bool kDebugFlowShortcutEnabled = true;
} // namespace

void GameScene::Initialize(const SceneServices &services) {
  // 読み込みの各ブロックの時間を測る(タイトルからの遷移で止まる原因の調査用)。
  // 全体の時間は、関数を抜けるときにログへ出る
  CpuTimer::Scope totalScope("GameScene.Initialize(total)");

  BaseScene::Initialize(services);

  auto* am = AbsoluteEngine::AssetManager::GetInstance();

  {
    CpuTimer::Scope scope("GameScene.hitEffects");
    hitEffects_.Initialize();
  }

  {
    CpuTimer::Scope scope("GameScene.noise0.png");
    // ディゾルブのマスクテクスチャのロード
    texNoise0_ = am->Load<TextureResource>(GameAssetPaths::kDissolveNoise);
    // ディゾルブのマスクテクスチャをレンダラーにセット
    if (texNoise0_) {
        Renderer::GetInstance()->SetDissolveMaskTexture(texNoise0_);
    }
  }

  {
    CpuTimer::Scope scope("GameScene.skybox(dds)");
    skybox_.Initialize(GameAssetPaths::kSkyboxDds);
  }

  {
    CpuTimer::Scope scope("GameScene.particleGroups");
    // パーティクルグループの初期化（連射ヒット時のアサートクラッシュ防止）
    ParticleManager::GetInstance()->CreateParticleGroup(GameParticleGroups::kDefaultName, GameAssetPaths::kDefaultParticleTexture, GameParticleGroups::kDefaultMax);
    ParticleManager::GetInstance()->CreateParticleGroup(GameParticleGroups::kExplosionName, GameAssetPaths::kExplosionParticleTexture, GameParticleGroups::kExplosionMax);
  }

  // レールカメラの初期化
  auto camera = std::make_shared<GameCamera>();
  camera->Initialize();
  camera->SetPerspective(kGameCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kGameCameraNear, kGameCameraFar);
  SetMainCamera(camera);

  // デバッグカメラ初期化（EditorCameraを、右ドラッグ不要で常時動ける自由視点モードで使う）
  debugCamera_ = std::make_unique<AbsoluteEngine::EditorCamera>();
  debugCamera_->Initialize();
  debugCamera_->SetPerspective(kGameCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kGameCameraNear, kGameCameraFar);
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
  {
    CpuTimer::Scope scope("GameScene.LoadEditorScene");
    LoadEditorScene();
  }
  // LoadEditorScene の中身の内訳(プレハブ読み込み、モデル読み込みの回数と合計)
  CpuTimer::FlushAccumulated();

  // -----------------------------------------------------------------------
  // 【バグ修正】エディタで配置した敵への onDestroyed コールバック一括登録
  // SpawnManager 経由で生成した敵は Spawn 時にコールバックを登録するが、
  // JSON/エディタからロードした敵にはその処理が走らないため、ここで補完する。
  // -----------------------------------------------------------------------
  for (const auto& obj : GetRootObjects()) {
      RegisterEnemyCallbacks(obj);
  }

  // シーン開始直後の初期視点がワープしないように1度更新して位置を確定させる
  GetMainCamera()->Update(*GetInput());

  // プレイヤーを探す（ロードされたデータにあるか）
  playerObj_ = FindPlayerObject_();

  // プレイヤー周りの準備(竜モデルの読み込みを含む)の時間を測る。
  // Scope を optional に入れて、この区間の終わりで明示的に閉じる
  std::optional<CpuTimer::Scope> playerScope;
  playerScope.emplace("GameScene.playerSetup(model+Fit)");
  // プレイヤーがいなければ生成
  if (!playerObj_) {
      playerObj_ = std::make_shared<AbsoluteEngine::GameObject>("Player");
      auto playerModelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
      playerModelComp->LoadModel("resources/app/player/player.obj");
      playerObj_->AddComponent(std::move(playerModelComp));
      AddRootObject(playerObj_);
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
  // 自機のモデルを、蝶に差し替える。
  // resources/editor/scenes/Game.json 上が別のモデルを指していても、実行時に必ず蝶にする。
  // 大きさ(翼幅)と中心は、モデルの頂点から求める(モデルを替えても大きさがそろう)
  // -----------------------------------------------------------------------
  {
      auto* playerModelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>();
      if (!playerModelComp) {
          auto newModelComp = std::make_unique<AbsoluteEngine::ModelComponent>();
          playerObj_->AddComponent(std::move(newModelComp));
          playerModelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>();
      }
      // JSON がすでに同じモデルを指しているときは、読み込み直さない。
      // 読み込み直すと、直前のインスタンスを同じフレームの更新で破棄してしまい、アップロード中のテクスチャが先に消える
      if (playerModelComp->GetModelPath() != kPlayerModelPath) {
          playerModelComp->LoadModel(kPlayerModelPath);
      }
      playerModelComp->FitToWingspan(kPlayerWingspan);
      // 死亡の崩壊(ディゾルブ)に使う、蝶専用のマスクと、ディゾルブの制御用のコンポーネント(普通は無効。死亡演出が制御する)
      playerModelComp->LoadDissolveMask(GameAssetPaths::kPlayerDissolveMask);
      // 崩壊で翅が裂けて飛び散る破片。分け方と描画用のモデルは重いので、シーンの開始時に作る(翅の描画から消した三角形も、ここで元に戻る)
      {
          auto* assets = AbsoluteEngine::AssetManager::GetInstance();
          wingShatter_.Initialize(playerModelComp, assets->Load<TextureResource>(GameAssetPaths::kPlayerAtlas),
                                  assets->Load<TextureResource>(GameAssetPaths::kPlayerDissolveMask));
      }
      // 蝶の翅は薄く、両面が見える。Lambert のままだと、光の当たらない側(裏面、影の側)が真っ黒になるので、環境光つきの薄い面にする
      playerModelComp->SetLightingMode(ModelLightingMode::kThinSurface);
      if (!playerObj_->GetComponent<AbsoluteEngine::DissolveComponent>()) {
          playerObj_->AddComponent(std::make_unique<AbsoluteEngine::DissolveComponent>());
      }
      // 通常は羽ばたいている。アニメーションの切り替え(被弾など)は PlayerComponent 側で行う
      playerModelComp->PlayAnimation(PlayerAnimation::kFlap, true);
  }

  playerScope.reset(); // プレイヤー周りの区間の終わり

  {
    CpuTimer::Scope scope("GameScene.InitializePostProcess");
    Renderer::GetInstance()->InitializePostProcess(1280, 720);
  }

  // デフォルトのライトを探す
  bool hasLight = false;
  for (const auto& obj : GetRootObjects()) {
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
      AddRootObject(initialDirLight);
  }

  // レールカメラを探す
  bool hasRailCamera = false;
  for (const auto& obj : GetRootObjects()) {
      if (obj && obj->GetComponent<RailCameraComponent>()) {
          hasRailCamera = true;
          break;
      }
  }
  if (!hasRailCamera) {
      auto railCamObj = std::make_shared<AbsoluteEngine::GameObject>("RailCamera");
      auto rComp = std::make_unique<RailCameraComponent>();
      railCamObj->AddComponent(std::move(rComp));
      AddRootObject(railCamObj);
  }

  {
    CpuTimer::Scope scope("GameScene.HUD");
    // HUDの初期化
    gameHUD_ = std::make_unique<GameHUD>();
    gameHUD_->Initialize();
  }

  // -----------------------------------------------------------------------
  // タイムラインマネージャーにRailCameraComponentを登録する
  // LoadEditorScene() 後に rootObjects_ を走査して RailCameraComponent を探し、
  // BaseScene 経由でタイムラインと紐付ける
  // -----------------------------------------------------------------------
  for (const auto& obj : GetRootObjects()) {
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
  {
    CpuTimer::Scope scope("GameScene.LoadTimeline");
    LoadTimeline();
  }

#ifndef USE_IMGUI
  // ImGuiが無いビルド（Release）ではPlayボタンを押す手段が無いため、
  // タイトルからゲームシーンに来た時点で自動的にPlayモードへ入る。
  BackupScene();
  SetPlayMode(PlayMode::Play);
  PlayTimeline();
#endif

  // ステージ名の板(失敗しても、板が出ないだけで、ゲームは進められる)。
  // 見せ方を選んで、壊れるときの準備(雲なら、画像の読み取りとパフの配置。破片なら、ボロノイ分割)まで済ませる。
  // 重い処理なので、画面遷移の覆いの中(シーンを作るとき)に行い、演出が始まる瞬間に止まらないようにする
  stageNameBoard_.Initialize(kStageNameModelPath, kStageNameTexturePath, kPuffTexturePath);
  stageNameBoard_.SetStyle(stageNameStyle_, stageCloudSettings_, stageSubtitleCloudSettings_);
  // 雲の文字が壊れたあとの霧のグループ(柔らかい縁で重なるので、「柔らかいアルファ」にして、加算ではなく、アルファブレンドで描く)
  ParticleManager::GetInstance()->CreateParticleGroup(kStageCloudMistGroup, kPuffTexturePath, kStageCloudMistMaxInstances, true);
  stageNameBoard_.BuildFracture(kStageNameImpactPoint, stageNameBoardSettings_, stageNameFractureSettings_, stageNameFragmentSettings_);
  stageImpact_.Initialize(kImpactFlashModelPath);
  speedStreaks_.Initialize(kSpeedStreakModelPath);

  // 画面遷移の覆いの中で始まった場合は、晴れる間の助走(イントロ)を始める
  BeginIntro_();
}

void GameScene::Finalize() {
  MouseCapture::Apply(false); // 隠していたOSマウスカーソルを必ず元に戻す
  // 破片のために潰した翅の三角形を元へ戻す(蝶のモデルは、次のゲームでも共有される)
  wingShatter_.Reset();
}

void GameScene::BeginIntro_() {
  // 覆いの中で始まったときだけ行う(編集中の再読み込みなど、遷移なしで始まったときは通常どおり始める)
  introActive_ = IsSceneTransitioning();
  if (!introActive_ || GetMainCamera() == nullptr) {
    introActive_ = false;
    return;
  }

  // 自機の位置は、通常は更新の中でカメラの前へ置かれる。イントロ中は更新しないので、時間 0 で 1 度だけ更新して置く
  playerObj_ = FindPlayerObject_();
  if (playerObj_) {
    playerObj_->Update(0.0f);
    introBasePlayerPosition_ = playerObj_->GetTransform().translate;
    introBasePlayerRotation_ = playerObj_->GetTransform().rotate;
  }
  introBaseEye_ = GetMainCamera()->GetEye();
  introBaseTarget_ = GetMainCamera()->GetTarget();

  introRailSpeed_ = 0.0f;
  if (const auto *rail = FindRailCamera_()) {
    introRailSpeed_ = rail->GetSpeed();
  }
}

RailCameraComponent *GameScene::FindRailCamera_() const {
  for (const auto &obj : GetRootObjects()) {
    if (auto *rail = obj ? obj->GetComponent<RailCameraComponent>() : nullptr) {
      return rail;
    }
  }
  return nullptr;
}

void GameScene::UpdateIntro_(float deltaTime) {
  // 覆いが晴れ終わったら、助走を終えて通常の更新へ戻る
  if (!IsSceneTransitioning()) {
    FinishIntro_();
    return;
  }

  const GameIntroPose pose = GameIntroDirector::Evaluate(
      GetTransitionRevealProgress(), introRailSpeed_, GetTransitionRevealSeconds());

  // 進行方向(カメラの向き)の後ろへ戻した位置へ、カメラと自機を同じだけずらす。
  // 一緒に動かすので、画面の中の自機の位置は変わらず、背景(雲海・グリッド)だけが流れて見える
  Vector3 forward{introBaseTarget_.x - introBaseEye_.x, introBaseTarget_.y - introBaseEye_.y, introBaseTarget_.z - introBaseEye_.z};
  forward = Normalize(forward);
  // 進行方向の後ろへ戻し、さらに下へ下げる(雲の層の中から昇り始める)
  const Vector3 shift{-forward.x * pose.backOffset, -forward.y * pose.backOffset - pose.riseOffset, -forward.z * pose.backOffset};

  auto *camera = GetMainCamera();
  camera->SetEye({introBaseEye_.x + shift.x, introBaseEye_.y + shift.y, introBaseEye_.z + shift.z});
  camera->SetTarget({introBaseTarget_.x + shift.x, introBaseTarget_.y + shift.y, introBaseTarget_.z + shift.z});
  camera->SetPerspective(kGameCameraFovY * pose.fovScale, Renderer::GetInstance()->GetAspectRatio(), kGameCameraNear, kGameCameraFar);

  if (playerObj_) {
    playerObj_->GetTransform().translate = {introBasePlayerPosition_.x + shift.x, introBasePlayerPosition_.y + shift.y,
                                           introBasePlayerPosition_.z + shift.z};
    // 昇りの機首上げ(X まわりの負の回転で、機首が上を向く)
    Vector3 rotation = introBasePlayerRotation_;
    rotation.x -= pose.pitchUp;
    playerObj_->GetTransform().rotate = rotation;
  }
  UpdatePlayerModelOnly_(deltaTime);
}

void GameScene::UpdatePlayerModelOnly_(float deltaTime) {
  if (!playerObj_) {
    return;
  }
  if (auto *modelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>()) {
    modelComp->Update(deltaTime);
  }
}

void GameScene::UpdateStageStart_(float deltaTime) {
  stageStart_.Update(deltaTime);

  // 世界が止まっている間(俯瞰へ引く・保つ)は、自機のアニメーションだけ進める。
  // 世界が動く「戻る」段階では、通常の更新が自機のモデルも進める
  if (!stageStart_.IsWorldRunning()) {
    UpdatePlayerModelOnly_(deltaTime);
  }

  // ステージ名の板: 段階に合わせて出し入れし、板を割る時刻になったら割る。
  // 割れる瞬間(自機が板に触れた瞬間)に、操作のロックを解く
  stageNameBoard_.SetAlpha(CalcStageNameAlpha(stageStart_.GetPhase(), stageStart_.GetPhaseProgress()));
  if (stageStart_.HasReachedBreak() && !stageNameBoard_.IsBroken()) {
    stageNameBoard_.Break();
    // 突き破った瞬間の衝撃(白いフラッシュ)
    stageImpact_.Trigger(stageImpactSettings_);
    SetPlayerInputLocked_(StageStartDemoMode::kEnabled);
  }
  // 割れるまでは、板を段階に合わせた位置へ動かす(割れたあとの破片は、割れた位置から、自分の動きで飛ぶ)
  if (!stageNameBoard_.IsBroken()) {
    stageNameBoard_.Place(CalcStageNamePosition_(), stageNameForward_, stageNameBoardSettings_);
  }

  // 演出が終わったら、割れていなくても(板が読み込めなかったときなど)、必ず操作を戻す
  if (!stageStart_.IsActive()) {
    SetPlayerInputLocked_(StageStartDemoMode::kEnabled);
  }
}

void GameScene::PlaceStageNameBoard_() {
  // 自機は、カメラの正面 d ユニットの位置にいる(カーソルが中央のとき)。d は、スタート位置のカメラと自機から求める
  const Vector3 baseForward = Normalize({introBaseTarget_.x - introBaseEye_.x, introBaseTarget_.y - introBaseEye_.y,
                                         introBaseTarget_.z - introBaseEye_.z});
  const Vector3 toPlayer{introBasePlayerPosition_.x - introBaseEye_.x, introBasePlayerPosition_.y - introBaseEye_.y,
                         introBasePlayerPosition_.z - introBaseEye_.z};
  const float playerDistance = Dot(toPlayer, baseForward);

  // 自機が板に触れる時刻(「戻る」段階の始まりから breakSeconds 後)のレール上の位置と向き。
  // 世界は「戻る」段階の始まりから動くので、そのぶんだけレールの進行度が進んだ位置になる
  Vector3 cameraPosition = introBaseEye_;
  Vector3 forward = baseForward;
  if (const auto *rail = FindRailCamera_(); rail != nullptr && rail->GetTotalLength() > 0.0f) {
    // 突き破るまでに進む距離: 世界は、ブースト(加速)で速くなりながら進むので、細かく区切って、速さを足し合わせて求める
    const float sampleSeconds = stageStartSettings_.breakSeconds / static_cast<float>(kBreakTravelSamples);
    float travel = 0.0f;
    for (int i = 0; i < kBreakTravelSamples; ++i) {
      const float returnSeconds = (static_cast<float>(i) + 0.5f) * sampleSeconds;
      travel += rail->GetSpeed() * WorldSpeedScale_(stageStart_.BoostAt(returnSeconds)) * sampleSeconds;
    }
    const float progress = rail->GetProgress() + travel / rail->GetTotalLength();
    rail->GetPointAndForward(progress, cameraPosition, forward);
  }

  // その時刻の自機の位置(カメラの正面)から、機首の分だけ前が、板に触れる位置
  const float contactDistance = playerDistance + stageNameBoardSettings_.contactOffset;
  stageNameAnchor_ = {cameraPosition.x + forward.x * contactDistance, cameraPosition.y + forward.y * contactDistance,
                      cameraPosition.z + forward.z * contactDistance};
  stageNameForward_ = forward;

  // 俯瞰を保つ間の位置: 俯瞰のカメラが、最終的にある位置の正面(画面の上寄り)。ここから弧を描いて、上の位置へ着く
  Vector3 holdEye{0.0f, 0.0f, 0.0f};
  Vector3 holdTarget{0.0f, 0.0f, 1.0f};
  StageStartDirector::ApplyPose(stageStart_.GetHoldPose(), introBaseEye_, introBaseTarget_, holdEye, holdTarget);
  stageNameHoldPosition_ = StageNamePath::InFrontOfCamera(holdEye, holdTarget, stageNameBoardSettings_.holdDistance,
                                                          stageNameBoardSettings_.holdAngle);

  stageNameBoard_.Place(CalcStageNamePosition_(), stageNameForward_, stageNameBoardSettings_);
}

float GameScene::WorldSpeedScale_(float boost) const {
  return 1.0f + (stageStartSettings_.boostSpeedScale - 1.0f) * boost;
}

int GameScene::CalcWorldSteps_() {
  // 速さの倍率を足し合わせ、整数の部分だけ今フレームの更新回数にして、小数の部分は次のフレームへ持ち越す。
  // 倍率が 1 のときは、毎フレームちょうど 1 回になる。2.5 のときは、2 回と 3 回が交互に来て、平均 2.5 回になる
  worldStepAccumulator_ += WorldSpeedScale_(stageStart_.GetBoost());
  const int steps = static_cast<int>(std::floor(worldStepAccumulator_));
  worldStepAccumulator_ -= static_cast<float>(steps);
  return steps;
}

StageStartPose GameScene::GetStageStartViewPose_() const {
  // 俯瞰のカメラのずれに、ブーストによる画角の広がりを重ねる(速く進むほど、画角が広がって、速さが強く見える)
  StageStartPose pose = stageStart_.GetPose();
  pose.fovScale *= 1.0f + (stageStartSettings_.boostFovScale - 1.0f) * stageStart_.GetBoost();
  return pose;
}

Vector3 GameScene::CalcStageNamePosition_() const {
  switch (stageStart_.GetPhase()) {
  case StageStartPhase::PullBack:
  case StageStartPhase::Hold: {
    // 俯瞰へ引く間は、引いていくカメラ(スタート位置のカメラに、今のずれを重ねたもの)の正面に、貼り付いたように付いてくる。
    // 俯瞰を保つ間は、カメラが止まるので、ワールドの同じ位置に留まる
    Vector3 eye{0.0f, 0.0f, 0.0f};
    Vector3 target{0.0f, 0.0f, 1.0f};
    StageStartDirector::ApplyPose(stageStart_.GetPose(), introBaseEye_, introBaseTarget_, eye, target);
    return StageNamePath::InFrontOfCamera(eye, target, stageNameBoardSettings_.holdDistance, stageNameBoardSettings_.holdAngle);
  }
  case StageStartPhase::Return: {
    // 竜の上を弧を描いて飛び越え、竜が触れる位置(前)へ降りる。竜が触れる時刻より前に着く
    const float duration = std::min(stageNameBoardSettings_.arcSeconds, stageStartSettings_.breakSeconds);
    const float u = duration > 0.0f ? stageStart_.GetReturnElapsedSeconds() / duration : 1.0f;
    const float s = SmoothStep01(u);

    // 始点から終点への直線(弦)の上の点に、弧の高さを足す
    const Vector3 chord = StageNamePath::ChordPoint(stageNameHoldPosition_, stageNameAnchor_, s);
    float lift = StageNamePath::ArcLift(stageNameBoardSettings_.arcHeight, s);

    // 板の上端が画面の上端を超えないよう、持ち上げる高さを抑える。
    // カメラは、戻る間に下がり、画角も変わるので、固定の高さでは画面の外へ出てしまう。
    // 画面に映るカメラ(今のカメラに、今の俯瞰のずれを重ねたもの)で、毎フレーム確かめる
    if (const auto *camera = GetMainCamera()) {
      const StageStartPose pose = GetStageStartViewPose_();
      Vector3 viewEye{0.0f, 0.0f, 0.0f};
      Vector3 viewTarget{0.0f, 0.0f, 1.0f};
      StageStartDirector::ApplyPose(pose, camera->GetEye(), camera->GetTarget(), viewEye, viewTarget);
      const float maxLift = StageNamePath::MaxLiftInView(chord, stageNameBoard_.HeightAboveImpact(stageNameBoardSettings_), viewEye,
                                                         viewTarget, kGameCameraFovY * pose.fovScale, stageNameBoardSettings_.screenLimit);
      lift = std::min(lift, maxLift);
    }
    return {chord.x, chord.y + lift, chord.z};
  }
  case StageStartPhase::Idle:
  default:
    return stageNameAnchor_;
  }
}

void GameScene::SetPlayerInputLocked_(bool locked) {
  if (!playerObj_) {
    return;
  }
  if (auto *player = playerObj_->GetComponent<PlayerComponent>()) {
    player->SetInputLocked(locked);
  }
}

void GameScene::FinishIntro_() {
  introActive_ = false;
  if (auto *camera = GetMainCamera()) {
    camera->SetEye(introBaseEye_);
    camera->SetTarget(introBaseTarget_);
    camera->SetPerspective(kGameCameraFovY, Renderer::GetInstance()->GetAspectRatio(), kGameCameraNear, kGameCameraFar);
  }
  if (playerObj_) {
    playerObj_->GetTransform().translate = introBasePlayerPosition_;
    playerObj_->GetTransform().rotate = introBasePlayerRotation_;
  }

  // イントロが終わったら、ステージ開始演出(俯瞰へ引き、戻りながら板を突き破る)へ続ける。基準の位置は、いま戻したスタート位置
  stageStart_.Begin(stageStartSettings_);

  // ステージ名の板を、自機が衝撃点を通る位置へ置く(割れ方の準備は、重いので、シーンを作るときに済ませてある)
  PlaceStageNameBoard_();

  // 板を割る(自機が板に触れる)までは、操作を受け付けない。自機が演出どおりに板へ向かって飛ぶようにする
  SetPlayerInputLocked_(true);
}

// -----------------------------------------------------------------------
// GetEditorViewCamera オーバーライド
// Draw() のカメラ選択分岐（isDebugCamera_ / GetPlayMode() による切り替え）と完全に一致させる。
// ここがズレると、Viewportに表示されている絵と、ギズモ・クリック判定の座標系が
// 食い違い、「実際のゲームカメラとは関係ない視点」に見えるバグになる。
// -----------------------------------------------------------------------
const Camera* GameScene::GetEditorViewCamera() const {
    if (isDebugCamera_) {
        if (GetPlayMode() == PlayMode::Edit && GetEditorCamera()) {
            return GetEditorCamera();
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

        // スコア加算
        scoreTracker_.OnEnemyKilled();

        // 編隊トラッキング：編隊が全滅したら全滅ボーナスを加算する(戻り値の初期メンバー数は、いまは使わない)
        if (formationComp) {
            scoreTracker_.OnFormationMemberKilled(formationComp->GetFormationId());
        }
    };
}

std::shared_ptr<AbsoluteEngine::GameObject> GameScene::FindPlayerObject_() const {
  for (const auto& obj : GetRootObjects()) {
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

void GameScene::UpdatePostEffects_(float deltaTime) {
  PostEffectDirector::FrameState state;
  state.isBossPhase = phase_ == GamePhase::Boss;
  // ステージ開始演出の加速(ブースト)の間、画面にブラーを掛ける。ブラーの中心は、画面の中心(自機のいる辺り)
  state.speedBlur = stageStart_.GetBoost();
  if (playerObj_) {
      if (auto* pComp = playerObj_->GetComponent<PlayerComponent>()) {
          state.hasPlayer     = true;
          state.playerHp      = pComp->GetHp();
      }
  }
  postEffects_.Update(deltaTime, state);
}

void GameScene::UpdateMouseCapture_() {
  // 展示中は、カーソルを隠さず、閉じ込めもしない(マウス操作を受け付けない)
  if (ExhibitionMode::kEnabled) {
      MouseCapture::Apply(false);
      return;
  }

  // Development では ImGui のエディタを操作する必要があるため、Escでいつでも解除/再開できる
  if (GetInput() && GetInput()->TriggerKey(DIK_ESCAPE)) {
      mouseCaptureSuspended_ = !mouseCaptureSuspended_;
  }

  const bool isInGame = GetPlayMode() == PlayMode::Play && !isDebugCamera_
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
  RemoveRootObjectsIf(isExpired);
}

void GameScene::SpawnBoss_() {
  // ボス出現演出（PostEffect加点要素: GaussianFilter）
  postEffects_.OnBossIntro();

  auto bossObj = std::make_shared<AbsoluteEngine::GameObject>("Boss");
  Vector3 forward = GetMainCamera()->GetForward();
  // 初期出現位置。以降はBossComponent::UpdateMovement()が毎フレーム
  // カメラの前方に追従＋左右上下の揺れを加えて上書きするため、
  // ここは最初の1フレーム分の暫定位置（BossComponent::kHoverDistanceFromCameraと合わせておく）。
  constexpr float kBossInitialDistanceFromCamera = 36.0f;
  const Vector3 cameraEye = GetMainCamera()->GetEye();
  bossObj->GetTransform().translate = {
      cameraEye.x + forward.x * kBossInitialDistanceFromCamera,
      cameraEye.y + forward.y * kBossInitialDistanceFromCamera,
      cameraEye.z + forward.z * kBossInitialDistanceFromCamera
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

  AddRootObject(bossObj);
}

void GameScene::CheckDebugFlowShortcut_() {
  if (!kDebugFlowShortcutEnabled || !GetInput()) {
    return;
  }
  // すでに終了の要求を出している間は受け付けない(二重に要求しない)
  if (phase_ != GamePhase::InProgress && phase_ != GamePhase::Boss) {
    return;
  }
  if (GetInput()->TriggerKey(DIK_RETURN)) {
    phase_ = GamePhase::Clear;
    RequestSceneChange(SceneId::Clear);
  } else if (GetInput()->TriggerKey(DIK_BACK)) {
    // 本番と同じ死亡の経路(Dying)を通す。HP を 0 にして、CheckGameEnd_ が死亡演出を始める
    KillPlayer_();
  }
}

void GameScene::UpdateExhibitionFlow_(float deltaTime) {
  // 死亡演出の最中や、終了の要求を出している間は何もしない(二重に死なせない)
  if (phase_ != GamePhase::InProgress && phase_ != GamePhase::Boss) {
    return;
  }
  // スタート演出の最中は数えない(終わってから、kGameHoldSec 秒たったら、自機を死なせる)
  if (stageStart_.IsActive()) {
    return;
  }
  exhibitionHoldTimer_ += deltaTime;
  if (exhibitionHoldTimer_ < ExhibitionMode::kGameHoldSec) {
    return;
  }
  // 本物の死亡処理を通す(HP を 0 にする。CheckGameEnd_ が死亡演出を始め、演出の終わりにゲームオーバーへの遷移を要求する)
  KillPlayer_();
}

void GameScene::UpdateStageStartDemoFlow_(float deltaTime) {
  // 演出の最中は数えない。終了の要求を出している間も何もしない(二重に要求しない)
  if (stageStart_.IsActive() || stageStartDemoReturnRequested_) {
    return;
  }
  if (phase_ != GamePhase::InProgress && phase_ != GamePhase::Boss) {
    return;
  }
  // 演出が終わってからの時間を数え、一定時間たったらタイトルへ戻る
  stageStartDemoTimer_ += deltaTime;
  if (stageStartDemoTimer_ < StageStartDemoMode::kReturnToTitleDelaySec) {
    return;
  }
  stageStartDemoReturnRequested_ = true;
  RequestSceneChange(SceneId::Title);
}

void GameScene::KillPlayer_() {
  if (!playerObj_) return;
  if (auto* pComp = playerObj_->GetComponent<PlayerComponent>()) {
      pComp->TakeDamage(pComp->GetMaxHp());
  }
}

void GameScene::BeginDying_() {
  phaseBeforeDeath_ = phase_;
  phase_ = GamePhase::Dying;

  PlayerDeathStart start;
  float railSpeed = 0.0f;
  if (const auto* rail = FindRailCamera_()) {
      railSpeed = rail->GetSpeed();
  }
  if (playerObj_) {
      start.position = playerObj_->GetTransform().translate;
      start.rotate = playerObj_->GetTransform().rotate;
      if (auto* pComp = playerObj_->GetComponent<PlayerComponent>()) {
          pComp->SetDying(true); // 追従・発射・ロックオンを止める(アニメーションの切り替えだけ進む)
      }
  }
  if (auto* camera = GetMainCamera()) {
      start.forward = Normalize(camera->GetForward());
  }
  start.railSpeed = railSpeed;

  // 落下は、雲の上面(いまのシーンの雲海の層の上面)へ着く時刻から逆算する
  PlayerDeathSettings settings = deathSettings_;
  settings.landingHeight = volumeCloudSea_.layerTop;
  deathDirector_.Begin(settings, start);

  // 死亡カメラ: 各キーフレームの時刻は、サブフェーズの終わりに合わせる(演出の長さを変えても、カメラが追従する)
  if (auto* camera = GetMainCamera()) {
      const std::array<float, DeathCameraDefaults::kKeyCount> keyTimes{
          0.0f,
          deathDirector_.GetPhaseEndTime(PlayerDeathPhase::HitStop),
          deathDirector_.GetPhaseEndTime(PlayerDeathPhase::Crumble),
          deathDirector_.GetPhaseEndTime(PlayerDeathPhase::Fall),
          deathDirector_.GetPhaseEndTime(PlayerDeathPhase::Impact),
          deathDirector_.GetPhaseEndTime(PlayerDeathPhase::Impact) + deathCameraSettings_.coverHoldSeconds};
      deathCamera_.Begin(deathCameraSettings_, camera->GetEye(), camera->GetTarget(), start.position, keyTimes);
  }
}

void GameScene::UpdateDying_(float deltaTime) {
  // スキップ: 0 キーかパッドの Start。展示中は受け付けない
  bool skipRequested = false;
  if (GetInput()) {
      skipRequested = GetInput()->TriggerKey(kDeathSkipKey) || GetInput()->WasPadPressed(kDeathSkipPadButton);
  }
  deathDirector_.Update(deltaTime, !ExhibitionMode::kEnabled, skipRequested);

  // 世界の時間の速さは、演出クラスが決める(ヒットストップで 0、そのあとスローモーション)
  SetWorldTimeScale(deathDirector_.GetWorldTimeScale());

  // 崩壊が始まる瞬間に、蝶をきりもみへ切り替える(スキップで飛んだときも 1 回出る)
  if (deathDirector_.ConsumeCrumbleStart() && playerObj_) {
      if (auto* pComp = playerObj_->GetComponent<PlayerComponent>()) {
          pComp->PlayDeathTumble();
      }
  }

  // Cover へ入ったら、ゲームオーバーへの遷移を要求する(1 回だけ)。覆いは、遷移が画面を覆う。演出クラスは、遷移の間も進み続ける
  // (デバッグで、遷移を要求せずに演出を保つ設定のときは、要求しない)
  if (deathDirector_.ConsumeCoverStart() && !debugHoldAtCover_) {
      phase_ = GamePhase::GameOver;
      RequestSceneChange(SceneId::GameOver);
  }
}

void GameScene::RemoveBulletsDuringDeath_() {
  for (const auto& obj : GetRootObjects()) {
      if (!obj) continue;
      const std::string& tag = obj->GetTag();
      if (tag == "PlayerBullet" || tag == "EnemyBullet") {
          obj->Destroy(); // 次の更新の終わりに、シーンから取り除かれる(それまでに描かれない: 更新は描画の前)
      }
  }
}

bool GameScene::IsHiddenDuringDeath_(const AbsoluteEngine::GameObject& obj) const {
  if (!deathCamera_.IsActive()) return false;
  const std::string& tag = obj.GetTag();
  if (tag != "Enemy" && tag != "Boss") return false;

  const Vector3& p = obj.GetTransform().translate;
  auto distanceSquared = [&](const Vector3& q) {
      return (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z);
  };
  const DeathCameraPose& pose = deathCamera_.GetPose();
  if (distanceSquared(pose.eye) < kDeathHideEnemyRadiusFromCamera * kDeathHideEnemyRadiusFromCamera) return true;
  if (playerObj_ && distanceSquared(playerObj_->GetTransform().translate) < kDeathHideEnemyRadiusFromPlayer * kDeathHideEnemyRadiusFromPlayer) return true;
  return false;
}

void GameScene::ReviveForDebug_() {
  // 遷移を要求したあと(GameOver)や、死亡していないときは、できない
  if (phase_ != GamePhase::Dying) return;
  deathDirector_.Reset();
  deathCamera_.Reset();
  wingShatter_.Reset();
  SetWorldTimeScale(1.0f);
  phase_ = phaseBeforeDeath_;
  if (playerObj_) {
      if (auto* pComp = playerObj_->GetComponent<PlayerComponent>()) {
          pComp->Revive();
      }
      if (auto* dissolve = playerObj_->GetComponent<AbsoluteEngine::DissolveComponent>()) {
          dissolve->enable = false;
      }
  }
}

void GameScene::CheckDebugReviveShortcut_() {
  if (!kDebugFlowShortcutEnabled || ExhibitionMode::kEnabled || !GetInput()) return;
  if (GetInput()->TriggerKey(kDebugReviveKey)) {
      ReviveForDebug_();
  }
}

void GameScene::ApplyDeathPose_() {
  if (!playerObj_ || !deathDirector_.IsActive()) return;
  const PlayerDeathPose pose = deathDirector_.GetPlayerPose();
  playerObj_->GetTransform().translate = pose.position;
  playerObj_->GetTransform().rotate = pose.rotate;

  // 蝶の朽ち方(ディゾルブ)。ModelComponent が、毎フレームこの値を描画に渡す
  if (auto* dissolve = playerObj_->GetComponent<AbsoluteEngine::DissolveComponent>()) {
      const PlayerDeathDissolve state = deathDirector_.GetDissolve();
      dissolve->enable = state.enabled;
      dissolve->threshold = state.threshold;
      dissolve->edgeRange = state.edgeRange;
      dissolve->edgeColor = state.edgeColor;
  }
}

void GameScene::CheckGameEnd_() {
  if (playerObj_) {
      auto pComp = playerObj_->GetComponent<PlayerComponent>();
      // HP が 0 になったら、死亡演出を始める。すでに演出中・終了の要求済みのときは、始めない
      if (pComp && pComp->IsDead() && (phase_ == GamePhase::InProgress || phase_ == GamePhase::Boss)) {
          BeginDying_();
      }
  }

  if (phase_ == GamePhase::Boss) {
      const bool bossAlive = std::any_of(GetRootObjects().begin(), GetRootObjects().end(),
          [](const auto& obj) { return obj && obj->GetComponent<BossComponent>(); });
      if (!bossAlive) {
          phase_ = GamePhase::Clear;
          RequestSceneChange(SceneId::Clear);
      }
  }
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

  const bool isLockingMode = pComp->GetLockOn().IsLockingMode();

  gameHUD_->Update(pComp->GetHp(), pComp->GetMaxHp(),
                   pComp->GetLockOn().GetLockedScreenPositions(GetMainCamera()), isLockingMode,
                   pComp->GetCursorPos(), hudCam,
                   pComp->GetNearReticleWorldPos(), pComp->GetMidReticleWorldPos(), pComp->GetFarReticleWorldPos());
}

void GameScene::Update() {
  const float deltaTime = 1.0f / 60.0f;
  time_ += deltaTime;

  // ステージ名の破片の動き。ステージ開始演出が終わったあとも、飛んでいる間は動かし続ける
  stageNameBoard_.Update(deltaTime);
  stageImpact_.Update(deltaTime);

  playerObj_ = FindPlayerObject_();

  // イントロ(画面遷移の覆いが晴れる間)は、世界の進行を止め、見た目だけの助走を動かす。
  // 晴れ終わった(UpdateIntro_ が終了した)フレームから、通常の更新に入る
  if (introActive_) {
      UpdateIntro_(deltaTime);
      if (introActive_) {
          return;
      }
  }

  // ステージ開始演出(俯瞰へ引き、戻りながら板を突き破る)。
  // 俯瞰へ引く・保つ間は、世界の進行を止める。「戻る」段階からは、世界(レール・敵・自機)を動かすので、通常の更新へ進む
  if (stageStart_.IsActive()) {
      UpdateStageStart_(deltaTime);
      if (stageStart_.IsActive() && !stageStart_.IsWorldRunning()) {
          return;
      }
  }

  CheckDebugReviveShortcut_();

  // 死亡演出(実時間)。世界の時間の速さを、ここで決めてから、世界を更新する
  if (deathDirector_.IsActive()) {
      UpdateDying_(deltaTime);
  }

  // ポストエフェクトの調停（Editモードでもタイマーは進める）
  UpdatePostEffects_(deltaTime);

  // ステージ開始演出の加速の間は、世界の更新(タイムライン、敵、自機、当たり判定)を、1 フレームに複数回回して速くする。
  // 通常は 1 回。エディット中は、速くしない
  const int worldSteps = GetPlayMode() == PlayMode::Play ? CalcWorldSteps_() : 1;
  for (int i = 0; i < worldSteps; ++i) {
      UpdateEditor();
  }

  UpdateMouseCapture_();

  if (GetPlayMode() == PlayMode::Play) {
#ifdef USE_IMGUI
      bool isDragging = ImGui::GetDragDropPayload() != nullptr;
#else
      bool isDragging = false;
#endif

      if (isDebugCamera_) {
          if (!isDragging) debugCamera_->Update(*GetInput());
      } else {
          // ゲーム（レール）カメラ進行
          CameraContext ctx{};
          ctx.deltaTime = deltaTime;
          GetMainCamera()->SetContext(ctx);
          if (!isDragging && phase_ == GamePhase::InProgress) GetMainCamera()->Update(*GetInput());
      }

      // 骨のデバッグ表示トグル（Bキー）
      if (!ExhibitionMode::kEnabled && GetInput() && GetInput()->TriggerKey(DIK_B)) {
          showDebugSkeleton_ = !showDebugSkeleton_;
      }

      // ヒットエフェクトはイベントコールバック（onDestroyed）によって
      // EnemyComponent::Update 内から自動的に発火されるため、ここでのポーリングは不要。

      RemoveExpiredObjects_();

      // パーティクルの更新（爆発エフェクト等）
      // 世界の時間の速さに合わせる(ヒットストップで止まり、スローモーションで遅くなる)
      const float worldDeltaTime = deltaTime * GetWorldTimeScale();
      ParticleManager::GetInstance()->Update(worldDeltaTime);

      // EffectManagerの更新（RingEffectなど）
      EffectManager::GetInstance()->Update(worldDeltaTime, GetGameplayCamera_());

      // ボスフェーズ移行
      float railProgress = 0.0f;
      for (const auto& obj : GetRootObjects()) {
          if (auto comp = obj->GetComponent<RailCameraComponent>()) {
              railProgress = comp->GetProgress();
              break;
          }
      }
      if (phase_ == GamePhase::InProgress && railProgress >= 1.0f) {
          phase_ = GamePhase::Boss;
          SpawnBoss_();
      }

      if (StageStartDemoMode::kEnabled) {
          UpdateStageStartDemoFlow_(deltaTime);
      } else if (ExhibitionMode::kEnabled) {
          UpdateExhibitionFlow_(deltaTime);
      } else {
          CheckDebugFlowShortcut_();
      }
      CheckGameEnd_();
      ApplyDeathPose_();
      // 崩壊の破片(解放、移動)。蝶の今のポーズ・位置を使うので、位置を決めたあとに進める
      if (deathDirector_.IsActive()) {
          wingShatter_.Update(deltaTime, deathDirector_.GetCrumbleProgress(), deathDirector_.GetPlayerVelocity());
          // 死亡カメラ(蝶の位置を追う)と、弾の停止(世界の更新で出た弾は、描かれる前にここで消す)
          if (playerObj_) {
              // 注視点は、本体と、解放された破片の重心との中間(破片が散っても、主役を見失わない)。
              // 破片が離れるほど、カメラを引いて、両方を画面に収める
              const Vector3 body = playerObj_->GetTransform().translate;
              Vector3 focus = body;
              float spread = 0.0f;
              Vector3 shards{0.0f, 0.0f, 0.0f};
              if (wingShatter_.GetReleasedCentroid(shards)) {
                  focus = {(body.x + shards.x) * 0.5f, (body.y + shards.y) * 0.5f, (body.z + shards.z) * 0.5f};
                  spread = std::sqrt((body.x - shards.x) * (body.x - shards.x) + (body.y - shards.y) * (body.y - shards.y) +
                                     (body.z - shards.z) * (body.z - shards.z));
              }
              deathCamera_.Update(deltaTime, focus, spread);
          }
          RemoveBulletsDuringDeath_();
      }
      UpdateHud_();
  }

  // シーンの自動セーブ（Editモード専用。Play中に動的スポーンされた敵/弾/ボスを含む
  // 現在の rootObjects_ をそのまま保存してしまうとレベルデータが壊れるため、
  // 実プレイ中（常にPlayMode::Play）は絶対に走らせない）
  if (GetPlayMode() == PlayMode::Edit) {
      bool shouldSave = false;
      for (const auto& obj : GetRootObjects()) {
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

  // ステージ開始演出の間は、俯瞰のカメラのずれを、描画の間だけ重ねる(この関数を抜けるとき、元のカメラへ戻る)。
  // デバッグカメラで見ているときは、ずらさない
  const ScopedStageStartCameraOffset stageStartView(stageStart_.IsActive() && !isDebugCamera_ ? GetMainCamera() : nullptr,
                                                    GetStageStartViewPose_());

  // 死亡演出のカメラ(蝶を中心にした回り込み・急降下)。描画の間だけ差し替える。デバッグカメラのときは、差し替えない
  std::optional<ScopedCameraOverride> deathView;
  if (deathCamera_.IsActive() && !isDebugCamera_ && GetMainCamera() != nullptr) {
      const DeathCameraPose& pose = deathCamera_.GetPose();
      deathView.emplace(GetMainCamera(), pose.eye, pose.target, kGameCameraFovY * pose.fovScale, kGameCameraFovY, kGameCameraNear,
                        kGameCameraFar);
  }

  const Camera* activeCamera = nullptr;
  if (isDebugCamera_) {
      // Debug Camera ON: エディット中はEditorCamera、プレイ中はDebugCameraを使用する
      if (GetPlayMode() == PlayMode::Edit && GetEditorCamera()) {
          renderer->SetCamera(*GetEditorCamera());
          activeCamera = GetEditorCamera();
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
  const bool newMethod = seaCloudMode_ == SeaCloudMode::NewMethod;
  if (useCloudSea_) {
      if (newMethod) {
          renderer->DrawSeaSky(seaSky_); // 新方式: 空だけ(雲は、下のボリューム雲の層が描く)
      } else {
          renderer->DrawCloudSea(cloudSea_); // 旧方式: 空と雲
      }
  } else {
      skybox_.Draw();
  }

  // ボリューム雲。実際の描画は、物体を描き終えたあと(EndRenderScene)に行われる。毎フレーム依頼する。
  // 重なるときは奥から手前の順に追加する: 雲海の層(下)→ 箱の雲。太陽の向きは背景と同じにして、光の向きをそろえる
  const Vector3 sunDirection = newMethod ? seaSky_.SunDirection() : cloudSea_.SunDirection();
  if (newMethod) {
      renderer->AddVolumeCloud(volumeCloudSea_, sunDirection, time_);
  }
  if (useVolumeCloud_) {
      renderer->AddVolumeCloud(volumeCloud_, sunDirection, time_);
  }

  // エディタ上で配置したオブジェクト群の描画
  for (auto& obj : GetRootObjects()) {
      if (!obj || IsHiddenDuringDeath_(*obj)) continue;
      obj->Draw();
  }

  // 崩壊の破片(蝶と同じテクスチャ・マスク・しきい値で、縁から朽ちながら飛ぶ)
  wingShatter_.Draw(deathDirector_.GetDissolve());

  // ステージ名の板(ステージ開始演出の間だけ、透明度が 0 より大きくなる)
  stageNameBoard_.Draw(renderer);
  if (showDebugFracture_) {
      stageNameBoard_.DrawDebugCells(renderer);
  }


  // EffectManager（RingEffect等）の描画
  EffectManager::GetInstance()->Draw();

  // パーティクルの描画（爆発エフェクト等）。「柔らかいアルファ」のグループ(雲の文字の霧)は、加算ではなく、アルファブレンドで別に描く。
  // 霧は、深度テストありで描く(竜や板より奥にある粒は、隠れる)
  ParticleManager::GetInstance()->Draw(BlendMode::Add, false, ParticleDrawFilter::NormalOnly);
  ParticleManager::GetInstance()->Draw(BlendMode::Alpha, true, ParticleDrawFilter::SoftAlphaOnly);

  // GPU Particleの描画（加点要素: GPU Particle拡張）
  Renderer::GetInstance()->DrawGPUParticles(BlendMode::Add);

  // ステージ開始演出の速度線(ブーストの強さで濃くなる)と、突き破った瞬間の白いフラッシュ。
  // どちらも、描画するカメラ(上でずらしたあとのもの)に合わせて描く。フラッシュは、カメラのすぐ前の板なので、最後に描く
  if (!isDebugCamera_) {
      if (const auto *camera = GetMainCamera()) {
          const Vector3 flowAxis = Normalize({introBaseTarget_.x - introBaseEye_.x, introBaseTarget_.y - introBaseEye_.y,
                                              introBaseTarget_.z - introBaseEye_.z});
          speedStreaks_.Draw(renderer, camera->GetEye(), flowAxis, stageStart_.GetBoost(), speedStreakSettings_);
          stageImpact_.DrawFlash(renderer, camera->GetEye(), camera->GetTarget());
      }
  }

  // --- デバッグラインの描画 ---
  if (showDebugRail_) {
      for (const auto& obj : GetRootObjects()) {
          if (auto* rComp = obj->GetComponent<RailCameraComponent>()) {
              rComp->DrawDebugLine(renderer);
              break;
          }
      }
  }

  // グリッドの描画（デバッグ用。雲海の背景では、雲の層の中から昇る間に空へ線として見えてしまうので、既定では描かない）
  if (showDebugGrid_) {
      renderer->DrawGrid(500.0f, 50, Vector4{0.2f, 0.4f, 0.8f, 0.5f});
  }

  // 骨のデバッグ表示（Bキートグル）
  if (showDebugSkeleton_ && playerObj_) {
      if (auto* modelComp = playerObj_->GetComponent<AbsoluteEngine::ModelComponent>()) {
          if (auto* instance = modelComp->GetModelInstance()) {
              instance->DrawSkeleton();
          }
      }
  }

  renderer->RenderPrimitives();

  // HUD等の2Dスプライト描画(ステージ開始演出の間は、俯瞰のカメラとレティクルの位置が合わないので、出さない)
  if (GetPlayMode() == PlayMode::Play && gameHUD_ && !stageStart_.IsActive() && !deathDirector_.IsActive()) {
      gameHUD_->Draw();
  }

  Matrix4x4 projInverse;
  if (isDebugCamera_) {
      // Debug Camera ON: エディット中はEditorCamera、プレイ中はDebugCameraのプロジェクション逆行列を使用
      if (GetPlayMode() == PlayMode::Edit && GetEditorCamera()) projInverse = Inverse(GetEditorCamera()->GetProjectionMatrix());
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

    if (ImGui::CollapsingHeader("Death Director")) {
        // 死亡演出の確認用。設定は、次に死んだとき(Begin)に反映される。「Hold at Cover」を入れると、遷移を要求せず、演出をその場で保つ
        ImGui::Text("Phase: %d  Elapsed: %.2f s  World time scale: %.2f", static_cast<int>(deathDirector_.GetPhase()),
                    deathDirector_.GetElapsed(), GetWorldTimeScale());
        if (ImGui::Button("Kill player (BACKSPACE)")) KillPlayer_();
        ImGui::SameLine();
        if (ImGui::Button("Revive (9)")) ReviveForDebug_();
        ImGui::Checkbox("Hold at Cover (no scene change)", &debugHoldAtCover_);
        ImGui::Checkbox("Hold player position (debug)", &deathSettings_.debugHoldPlayerPosition);
        ImGui::Text("Landing height (cloud top): %.1f", volumeCloudSea_.layerTop);
        ImGui::SeparatorText("Timing");
        ImGui::SliderFloat("Hit stop (s)", &deathSettings_.hitStopSeconds, 0.0f, 0.5f);
        ImGui::SliderFloat("Crumble (s)", &deathSettings_.crumbleSeconds, 0.2f, 3.0f);
        ImGui::SliderFloat("Fall (s)", &deathSettings_.fallSeconds, 0.2f, 4.0f);
        ImGui::SliderFloat("Impact (s)", &deathSettings_.impactSeconds, 0.1f, 2.0f);
        ImGui::SliderFloat("Slow motion scale", &deathSettings_.slowMotionScale, 0.0f, 1.0f);
        ImGui::SliderFloat("Slow motion ramp (s)", &deathSettings_.slowMotionRampSeconds, 0.0f, 1.0f);
        ImGui::SliderFloat("Sink decay (1/s)", &deathSettings_.sinkDecayPerSecond, 0.0f, 6.0f);
        ImGui::SeparatorText("Camera keys (0 = start, 1 = hit stop end, 2 = crumble end, 3 = fall end, 4 = impact end, 5 = cover)");
        ImGui::SliderFloat("Cover hold (s)", &deathCameraSettings_.coverHoldSeconds, 0.0f, 4.0f);
        ImGui::SliderFloat("Follow sharpness", &deathCameraSettings_.followSharpness, 1.0f, 20.0f);
        for (int i = 1; i < DeathCameraDefaults::kKeyCount; ++i) {
            DeathCameraDefaults::Key& key = deathCameraSettings_.keys[i];
            ImGui::PushID(i);
            ImGui::Text("Key %d", i);
            ImGui::SliderFloat("azimuth", &key.azimuthDelta, -3.2f, 3.2f);
            ImGui::SliderFloat("elevation", &key.elevation, 0.0f, DeathCameraDefaults::kMaxElevation);
            ImGui::SliderFloat("distance", &key.distance, 0.5f, 40.0f);
            ImGui::SliderFloat("fov scale", &key.fovScale, 0.3f, 2.0f);
            ImGui::PopID();
        }
        if (ImGui::Button("Reset death settings")) {
            deathSettings_ = PlayerDeathSettings{};
            deathCameraSettings_ = DeathCameraSettings{};
        }
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Cloud Sea")) {
        ImGui::Checkbox("Use Cloud Sea (off = Skybox)", &useCloudSea_);
        int seaMode = static_cast<int>(seaCloudMode_);
        ImGui::RadioButton("Sea clouds: new method (volume layer)", &seaMode, static_cast<int>(SeaCloudMode::NewMethod));
        ImGui::RadioButton("Sea clouds: old method (height field)", &seaMode, static_cast<int>(SeaCloudMode::OldMethod));
        seaCloudMode_ = static_cast<SeaCloudMode>(seaMode);
        if (seaCloudMode_ == SeaCloudMode::NewMethod) {
            // 新方式: 空(背景)の設定。雲の設定は、下の「Volume Cloud」で調整する
            ImGui::SeparatorText("Sea Sky");
            ImGui::SliderFloat("Sun Azimuth", &seaSky_.sunAzimuthDeg, -kSeaSkySunAzimuthRange, kSeaSkySunAzimuthRange, "%.0f");
            ImGui::SliderFloat("Sun Elevation", &seaSky_.sunElevationDeg, kSeaSkySunElevationMin, kSeaSkySunElevationMax, "%.0f");
            ImGui::SliderFloat("Gap Darkness", &seaSky_.gapDarkness, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Floor Height", &seaSky_.floorHeight, kSeaSkyFloorHeightMin, kSeaSkyFloorHeightMax, "%.0f");
        } else {
            CloudSeaDebugUi::Draw(cloudSea_);
        }
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Volume Cloud")) {
        ImGui::Checkbox("Use Volume Cloud (box)", &useVolumeCloud_);
        // 調整する雲。新方式のときは、雲海の層と箱を選べる(旧方式のときは、箱だけ)
        const bool canEditSea = seaCloudMode_ == SeaCloudMode::NewMethod;
        if (canEditSea) {
            int editTarget = volumeCloudEditSea_ ? 0 : 1;
            ImGui::RadioButton("Edit: sea layer", &editTarget, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Edit: box", &editTarget, 1);
            volumeCloudEditSea_ = editTarget == 0;
        }
        VolumeCloudSettings &editing = (canEditSea && volumeCloudEditSea_) ? volumeCloudSea_ : volumeCloud_;
        VolumeCloudDebugUi::Draw(editing);
        VolumeCloudDebugUi::DrawQuality(Renderer::GetInstance()->GetVolumeCloudQuality(), editing);
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
        for (const auto& obj : GetRootObjects()) {
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
        for (const auto& obj : GetRootObjects()) {
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
    ImGui::Checkbox("Show Debug Grid", &showDebugGrid_);
    if (ImGui::Button("Reset Rail Camera")) {
        for (const auto& obj : GetRootObjects()) {
            if (auto rComp = obj->GetComponent<RailCameraComponent>()) {
                rComp->ResetProgress();
                break;
            }
        }
        CameraContext ctx{};
        ctx.deltaTime = 1.0f / 60.0f;
        GetMainCamera()->SetContext(ctx);
        if (GetInput()) GetMainCamera()->Update(*GetInput());
    }

    ImGui::End();
#endif
}