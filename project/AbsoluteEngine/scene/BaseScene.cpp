#include "BaseScene.h"
#include "SceneManager.h"
#include "../graphics/3d/model/ModelManager.h"
#include "../graphics/texture/TextureManager.h"
#include "LightNodeComponent.h"
#include "../base/AssetPath.h"
// エンジン機能用インクルード
#include "AbsoluteEngine/editor/EditorUIManager.h"
#include "AbsoluteEngine/editor/EditorCamera.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "CollisionManager.h"
#include "GameObject.h"
#include "Input.h"
#include "Renderer.h"
#include "../camera/ITimelineCamera.h"
#include "timeline/PrefabRegistry.h"
// タイムラインイベントプレビュー用（タスク16）
#include "timeline/SpawnEvent.h"
#include <algorithm>
#include <filesystem>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
// エディタ用カメラの投影パラメータ（アスペクト比は Renderer の現在値を使う）
constexpr float kEditorCameraFovY = 0.45f;
constexpr float kEditorCameraNear = 0.1f;
constexpr float kEditorCameraFar = 1000.0f;
} // namespace

BaseScene* BaseScene::activeScene_ = nullptr;

BaseScene::~BaseScene() {
    // SceneManagerが破棄後・次シーンのInitialize前の間に誰かがGetActiveScene()を呼んでも
    // 解放済みインスタンスへのダングリングポインタを返さないよう、破棄時に必ずクリアする
    if (activeScene_ == this) {
        activeScene_ = nullptr;
    }
}

void BaseScene::Initialize(const SceneServices &services) {
    services_ = services;
    activeScene_ = this;

    // カメラの初期化（ゲーム中も使用するためマクロ外で生成）
    editorCamera_ = std::make_unique<AbsoluteEngine::EditorCamera>();
    editorCamera_->Initialize();
    RefreshEditorCameraProjection();

    // タイムライン機能を使うシーンのみ TimelineManager/TimelineEditorWindow を生成する
    if (UsesTimeline()) {
        timelineManager_ = std::make_unique<AbsoluteEngine::TimelineManager>();
        timelineEditorWindow_ = std::make_unique<AbsoluteEngine::TimelineEditorWindow>();
        timelineEditorWindow_->SetManager(timelineManager_.get());
    }

    // プレハブレジストリの初期化スキャン
    const std::string prefabDir = AbsoluteEngine::AssetPath::ResolveEditableData("prefabs");
#ifndef USE_IMGUI
    // 配布ビルドでプレハブが無いと、敵のスポーンが黙って失敗する。気づけるよう止める
    if (!std::filesystem::exists(prefabDir)) {
        AbsoluteEngine::AssetPath::ReportMissingAndExit("prefab directory", prefabDir);
    }
#endif
    AbsoluteEngine::PrefabRegistry::GetInstance().ScanDirectory(prefabDir);

#ifdef USE_IMGUI
    // エディタUIマネージャーの初期化
    editorUIManager_ = std::make_unique<AbsoluteEngine::EditorUIManager>();
    // タイムラインマネージャをインスペクタにポイントする（タスク15）。未使用シーンではnullptrのまま
    editorUIManager_->SetTimelineManager(timelineManager_.get());
#endif
}

void BaseScene::RemoveRootObject(const std::shared_ptr<AbsoluteEngine::GameObject>& obj) {
  rootObjects_.erase(std::remove(rootObjects_.begin(), rootObjects_.end(), obj), rootObjects_.end());
}

void BaseScene::RemoveRootObjectsIf(
    const std::function<bool(const std::shared_ptr<AbsoluteEngine::GameObject>&)>& shouldRemove) {
  rootObjects_.erase(std::remove_if(rootObjects_.begin(), rootObjects_.end(), shouldRemove), rootObjects_.end());
}

const Camera* BaseScene::GetEditorViewCamera() const {
  return editorCamera_.get();
}

void BaseScene::RefreshEditorCameraProjection() {
  if (!editorCamera_) return;
  editorCamera_->SetPerspective(kEditorCameraFovY, Renderer::GetInstance()->GetAspectRatio(),
                                kEditorCameraNear, kEditorCameraFar);
}

bool BaseScene::DrawEditorCameraControls() {
  if (!editorCamera_) return false;
  AbsoluteEngine::EditorUIManager::DrawCameraControls(*editorCamera_);
  return true;
}

void BaseScene::SetEditorCameraShake(const Vector3 &offset) {
  if (!editorCamera_) return;
  editorCamera_->SetShakeOffset(offset);
  editorCamera_->UpdateMatrix();
}

void BaseScene::RequestSceneChange(const std::string &sceneId) {
  if (sceneManager_) {
    sceneManager_->RequestChange(sceneId);
  }
}

bool BaseScene::IsSceneTransitioning() const {
  return sceneManager_ != nullptr && sceneManager_->IsTransitioning();
}

float BaseScene::GetTransitionRevealProgress() const {
  return sceneManager_ != nullptr ? sceneManager_->GetRevealProgress() : 1.0f;
}

float BaseScene::GetTransitionRevealSeconds() const {
  return sceneManager_ != nullptr ? sceneManager_->GetRevealSeconds() : 0.0f;
}

AbsoluteEngine::CommandManager* BaseScene::GetCommandManager() const {
#ifdef USE_IMGUI
    if (editorUIManager_) return editorUIManager_->GetCommandManager();
#endif
    return nullptr;
}

void BaseScene::BackupScene() {
    backupSceneJson_ = AbsoluteEngine::SceneSerializer::SerializeToString(rootObjects_, true);
}

void BaseScene::RestoreScene() {
    rootObjects_.clear();
    AbsoluteEngine::SceneSerializer::DeserializeFromString(backupSceneJson_, rootObjects_);

    // タスク12: 復元後にタイムラインのカメラポインタを再接続する
    if (timelineManager_) {
        for (const auto& obj : rootObjects_) {
            if (!obj) continue;
            if (auto* railComp = obj->GetComponent<AbsoluteEngine::ITimelineCamera>()) {
                SetTimelineCamera(railComp);
                break;
            }
        }
    }
}

std::string BaseScene::GetSceneFilePath() const {
    if (sceneId_.empty()) return AbsoluteEngine::AssetPath::ResolveEditableData("editor/scenes/scene.json");
    return AbsoluteEngine::AssetPath::ResolveEditableData("editor/scenes/" + sceneId_ + ".json");
}

bool BaseScene::LoadEditorScene() {
    std::string filePath = GetSceneFilePath();
    if (!std::filesystem::exists(filePath)) {
#ifdef USE_IMGUI
        // エディタビルド: ファイルがない場合は空の状態（または派生先で初期化された状態）で保存して新規作成する
        SaveEditorScene();
        return false;
#else
        // 配布ビルド: 書き込みはしない。空のシーンで黙って進行せず、探した場所を表示して止める
        AbsoluteEngine::AssetPath::ReportMissingAndExit("scene file", filePath);
#endif
    }
    AbsoluteEngine::SceneSerializer::Deserialize(filePath, rootObjects_);
    return true;
}

void BaseScene::SaveEditorScene() {
#ifdef USE_IMGUI
    // 保存はエディタビルドのみ。配布ビルドではソース/配布物のJSONを書き換えない
    std::string saveDir = AbsoluteEngine::AssetPath::ResolveEditableData("editor/scenes/");
    std::filesystem::create_directories(saveDir);
    AbsoluteEngine::SceneSerializer::Serialize(GetSceneFilePath(), rootObjects_);
    // タイムラインデータも同時に保存してシーンとタイムラインのデータを完全同期する（タスクB）
    SaveTimeline();
#endif
}

void BaseScene::UpdateEditor() {
#ifdef USE_IMGUI
    // ドラッグ＆ドロップ中はカメラの操作をブロックする
    bool isDragging = ImGui::GetDragDropPayload() != nullptr;

    // エディタカメラの更新
    // Debug Camera トグルがOFFの派生クラスでは非表示のeditorCamera_が入力を奪わないようにする（タスクD）
    if (editorCamera_ && playMode_ == PlayMode::Edit && !isDragging && IsEditorCameraOperationEnabled()) {
        editorCamera_->Update(*services_.input);
    }
#endif

    const float deltaTime = 1.0f / 60.0f; // 固定のdeltaTime

    // エディットモードでもタイムラインだけが再生中の場合は更新する
    if (playMode_ == PlayMode::Edit && timelineManager_ && timelineManager_->GetPlayState() == AbsoluteEngine::TimelinePlayState::Playing) {
        UpdateTimeline(deltaTime);
    }

    // タイムラインイベント選択に応じたプレビューオブジェクト管理（タスク16）
    // エディットモード中のみ有効（プレイ中に仮オブジェクトが混入しないように）
    if (playMode_ == PlayMode::Edit && editorUIManager_ && timelineManager_) {
        AbsoluteEngine::ITimelineEvent* currentSelectedEvent = timelineManager_->GetSelectedEvent();

        // 選択イベントが切り替わったか、または選択解除された場合は古いプレビューを破棄する
        if (prevSelectedTimelineEvent_ != currentSelectedEvent) {
            // 古いプレビューオブジェクトを破棄する
            if (auto prevPreview = timelinePreviewObject_.lock()) {
                prevPreview->Destroy();
                // EditorUIManagerの選択も解除する
                editorUIManager_->SetSelectedObject(nullptr);
            }
            timelinePreviewObject_.reset();

            // 新しいイベントが選択された場合はプレビューオブジェクトを生成する
            if (currentSelectedEvent != nullptr) {
                // SpawnEvent の場合のみプレビューオブジェクトを生成する
                if (auto* spawnEv = dynamic_cast<AbsoluteEngine::SpawnEvent*>(currentSelectedEvent)) {
                    const std::string path = AbsoluteEngine::PrefabRegistry::GetInstance().ResolveId(spawnEv->prefabId_);
                    if (!path.empty()) {
                        // プレハブをロードして仮オブジェクトとして生成する
                        auto previewObj = AbsoluteEngine::SceneSerializer::LoadPrefab(path);
                        if (previewObj) {
                            // プレビューフラグを立ててシリアライズ対象から除外する
                            previewObj->SetTimelinePreview(true);
                            // スポーン位置に配置する
                            previewObj->GetTransform() = spawnEv->spawnTransform_;
                            // シーンに追加してギズモ操作対象に登録する
                            AddRootObject(previewObj);
                            editorUIManager_->SetSelectedObject(previewObj);
                            timelinePreviewObject_ = previewObj;
                        }
                    }
                }
            }
            prevSelectedTimelineEvent_ = currentSelectedEvent;
        }

        // プレビューオブジェクトが存在する場合はギズモ操作を SpawnEvent::spawnTransform_ に対して毎フレーム書き戻す
        if (auto previewObj = timelinePreviewObject_.lock()) {
            if (auto* spawnEv = dynamic_cast<AbsoluteEngine::SpawnEvent*>(currentSelectedEvent)) {
                // ギズモまたはインスペクタで変更された Transform をイベントの値に書き戻す
                spawnEv->spawnTransform_ = previewObj->GetTransform();
            }
        }
    }

    // プレイモード中のオブジェクトの更新
    if (playMode_ == PlayMode::Play) {
        // タイムラインをオブジェクトより先に更新する
        UpdateTimeline(deltaTime);

        // 追加されたオブジェクトでループが壊れないようインデックスで回す
        size_t count = rootObjects_.size();
        for (size_t i = 0; i < count; ++i) {
            auto obj = rootObjects_[i];
            if (obj && obj->IsActive()) {
                obj->Update(deltaTime);
            }
        }
        
        AbsoluteEngine::CollisionManager::GetInstance().Update(rootObjects_);
    }

    // ガベージコレクション（IsActive() == false なオブジェクトを削除）
    // Playモード限定ではなく毎フレーム・常時実行する。GameObject::Destroy()はisActive_を
    // falseにするだけで実際の削除はここで行われるため、Editモード限定（タイムラインの
    // トラック削除やEdit中のタイムラインプレビュー巻き戻しでSpawnEvent::Rewind()がDestroy()
    // する場合など）でもここを通さないと、破棄されたはずのオブジェクトがrootObjects_に
    // 残り続けて描画され続けてしまう（削除したはずのオブジェクトが消えない/再スポーンが
    // 別オブジェクトとして重なって見えるバグの原因だった）
    rootObjects_.erase(std::remove_if(rootObjects_.begin(), rootObjects_.end(), [](const std::shared_ptr<AbsoluteEngine::GameObject>& obj) {
        return !obj || !obj->IsActive();
    }), rootObjects_.end());
}

void BaseScene::DrawEditorUI() {
#ifdef USE_IMGUI
    // ツールバー（プレイモード切り替え）
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    
    if (playMode_ == PlayMode::Edit) {
        if (ImGui::Button("Play")) {
            BackupScene();
            playMode_ = PlayMode::Play;
            // タイムラインもシーンのPlayに合わせて現在位置から再生開始する
            if (timelineManager_) timelineManager_->Play();
        }
    } else if (playMode_ == PlayMode::Play) {
        if (ImGui::Button("Pause")) {
            playMode_ = PlayMode::Pause;
            // タイムラインも一時停止する
            if (timelineManager_) timelineManager_->Pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            RestoreScene();
            if (editorUIManager_) editorUIManager_->SetSelectedObject(nullptr);
            playMode_ = PlayMode::Edit;
            // タイムラインを停止・先頭にリセットする
            if (timelineManager_) timelineManager_->Stop();
        }
    } else if (playMode_ == PlayMode::Pause) {
        if (ImGui::Button("▶ Resume")) {
            playMode_ = PlayMode::Play;
            // タイムラインも再生再開する
            if (timelineManager_) timelineManager_->Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("■ Stop")) {
            RestoreScene();
            if (editorUIManager_) editorUIManager_->SetSelectedObject(nullptr);
            playMode_ = PlayMode::Edit;
            // タイムラインを停止・先頭にリセットする
            if (timelineManager_) timelineManager_->Stop();
        }
    }
    ImGui::End();

    // エディタUIの描画
    // 「Viewport##GameView」の描画（ゲーム画面のプレビュー表示）はPlay中も必要なため、
    // DrawUI()自体は常に呼ぶ（Editモード限定にすると、Playモード中に編集ツール一式ごと
    // ゲーム画面まで消えてしまう）。
    if (editorUIManager_ && editorCamera_) {
        AbsoluteEngine::EditorCamera* edCam = editorCamera_.get();
        // ギズモ・マウスピッキングは「実際に画面に描画されているカメラ」の行列を使う
        // （常にeditorCamera_を使うと、Debug Camera OFF時にGameCamera(レールカメラ)で
        //   描画されている絵とギズモ/クリック判定がズレて、意味不明な視点に見える不具合になる）
        const Camera* viewCam = GetEditorViewCamera();
        if (!viewCam) viewCam = editorCamera_.get();
        editorUIManager_->DrawUI(rootObjects_, viewCam->GetViewMatrix(), viewCam->GetProjectionMatrix(), edCam);

        // オートセーブの実行
        // 【バグ修正】ここにplayMode_のガードが無かったため、GameScene（常にPlayModeで
        // 起動）を実際にプレイ中でもマウス操作が偶然ギズモ等に当たって isModified_ が立つと、
        // 「プレイ中に動的生成された敵・弾・ボスを含む今の状態」がそのままシーンJSONへ
        // 上書き保存されてしまっていた（＝二度と正常にロードできない壊れたレベルになる）。
        // 保存はEditモードでの編集結果に対してのみ行う。
        if (playMode_ == PlayMode::Edit && editorUIManager_->ConsumeSceneModifiedFlag()) {
            SaveEditorScene();
        }
    }

    // タイムラインエディタウィンドウの描画
    // （USE_IMGUI ブロック内なのでリリースビルドでも安全）
    DrawTimelineEditorUI();

#endif
}

void BaseScene::ApplyEditorLightsToRenderer(Renderer* renderer) {
    if (!renderer) return;

    std::vector<DirLight> dirLights;
    std::vector<PointLight> pointLights;
    std::vector<SpotLight> spotLights;

    auto collectLights = [&](auto& self, const std::shared_ptr<AbsoluteEngine::GameObject>& obj) -> void {
        if (!obj) return;
        const auto& t = obj->GetTransform();

        auto lightComp = obj->GetComponent<AbsoluteEngine::LightNodeComponent>();
        if (lightComp && lightComp->type != AbsoluteEngine::LightNodeComponent::Type::None) {
            const auto& light = *lightComp;
            
            // Transformの回転から方向ベクトルを計算
            Matrix4x4 rotX = MakeRotateXMatrix(t.rotate.x);
            Matrix4x4 rotY = MakeRotateYMatrix(t.rotate.y);
            Matrix4x4 rotZ = MakeRotateZMatrix(t.rotate.z);
            Matrix4x4 rotMatrix = Multiply(Multiply(rotZ, rotX), rotY);
            
            Vector3 defaultDir = {0.0f, -1.0f, 0.0f};
            Vector3 dir = TransformNormal(defaultDir, rotMatrix);
            dir = Normalize(dir);

            if (light.type == AbsoluteEngine::LightNodeComponent::Type::Directional) {
                DirLight dl;
                dl.color = light.color;
                dl.intensity = light.intensity;
                dl.direction = dir;
                dl.enabled = true;
                dirLights.push_back(dl);
            } else if (light.type == AbsoluteEngine::LightNodeComponent::Type::Point) {
                PointLight pl;
                pl.color = light.color;
                pl.intensity = light.intensity;
                pl.radius = light.radius;
                pl.decay = light.decay;
                pl.position = t.translate; 
                pl.enabled = true;
                pointLights.push_back(pl);
            } else if (light.type == AbsoluteEngine::LightNodeComponent::Type::Spot) {
                SpotLight sl;
                sl.color = light.color;
                sl.intensity = light.intensity;
                sl.distance = light.distance;
                sl.decay = light.decay;
                sl.coneAngleDeg = light.coneAngleDeg;
                sl.position = t.translate;
                sl.direction = dir;
                sl.enabled = true;
                spotLights.push_back(sl);
            }
        }

        for (const auto& child : obj->GetChildren()) {
            self(self, child);
        }
    };

    for (const auto& obj : rootObjects_) {
        collectLights(collectLights, obj);
    }

    renderer->SetDirectionalLights(dirLights, true);
    renderer->SetPointLights(pointLights, true);
    renderer->SetSpotLights(spotLights, true);
}

// ============================================================
// タイムライン統合メソッド
// ============================================================

std::string BaseScene::GetTimelineFilePath() const {
    // シーンIDに "_timeline" を付加したJSONファイルを使用する
    // 例: "devScene" -> "resources/editor/scenes/devScene_timeline.json"
    const std::string baseName = sceneId_.empty() ? "scene" : sceneId_;
    return AbsoluteEngine::AssetPath::ResolveEditableData(
        "editor/scenes/" + baseName + "_timeline.json");
}

void BaseScene::SaveTimeline() {
#ifdef USE_IMGUI
    // UsesTimeline()がfalseのシーンではtimelineManager_が存在しないため何もしない
    if (!timelineManager_) return;

    // タイムラインデータをシーンIDに紐付けたJSONファイルに保存する（保存はエディタビルドのみ）
    const std::string filePath = GetTimelineFilePath();
    std::filesystem::create_directories(
        std::filesystem::path(filePath).parent_path().string());
    timelineManager_->SaveToFile(filePath);
#endif
}

void BaseScene::LoadTimeline() {
    if (!timelineManager_) return;

    // タイムラインデータを読み込む（エディタビルドでは、ファイルが無い場合は空の状態を維持）
    const std::string filePath = GetTimelineFilePath();
    if (std::filesystem::exists(filePath)) {
        timelineManager_->LoadFromFile(filePath);
    }
#ifndef USE_IMGUI
    else {
        // 配布ビルドで無いと、敵が一切出ないまま進行してしまう。探した場所を表示して止める
        AbsoluteEngine::AssetPath::ReportMissingAndExit("timeline file", filePath);
    }
#endif
    // 読み込み直後に必ず Stop()を呼び、シーン上のカメラ等を
    // タイムラインの 0.0s の状態に強制スナップする（タスクB）
    timelineManager_->Stop();
}

void BaseScene::SetTimelineCamera(AbsoluteEngine::ITimelineCamera* camera) {
    if (!timelineManager_) return;
    timelineManager_->SetCamera(camera);
}

void BaseScene::SetTimelineSpawnAheadDistance(float distance) {
    if (!timelineManager_) return;
    timelineManager_->SetSpawnAheadDistance(distance);
}

void BaseScene::UpdateTimeline(float deltaTime) {
    if (!timelineManager_) return;
    timelineManager_->Update(deltaTime);
}

void BaseScene::PlayTimeline() {
    if (!timelineManager_) return;
    timelineManager_->Play();
}

void BaseScene::DrawTimelineEditorUI() {
    // UsesTimeline()がfalseのシーンではウィンドウ自体が存在しないため描画しない
    if (!timelineEditorWindow_) return;

    // Debug/Game カメラ切替フラグを接続する（派生クラスがオーバーライドしていればそのポインタを渡す）
    timelineEditorWindow_->SetDebugCameraFlag(GetDebugCameraFlag());

    // トラック/イベントの追加・削除等を既存のオートセーブ経路に接続する
    if (editorUIManager_) {
        AbsoluteEngine::EditorUIManager* uiManager = editorUIManager_.get();
        timelineEditorWindow_->SetOnModifiedCallback([uiManager]() { uiManager->SetSceneModified(); });
    }

    timelineEditorWindow_->Draw(GetTimelineFilePath());
}
