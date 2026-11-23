#include "BaseScene.h"
#include "../graphics/3d/model/ModelManager.h"
#include "../graphics/texture/TextureManager.h"
#include "SceneLights.h"
#include "../base/AssetPath.h"
// エンジン機能用インクルード
#include "AbsoluteEngine/editor/EditorUIManager.h"
#include "EditorSession.h"
#include "AbsoluteEngine/editor/EditorCamera.h"
#include "AbsoluteEngine/scene/SceneSerializer.h"
#include "GameObject.h"
#include "Input.h"
#include "Renderer.h"
#include "Logger.h"
#include "DirectXCommon.h"
#include "../camera/ITimelineCamera.h"
#include "timeline/PrefabRegistry.h"
#include <algorithm>
#include <filesystem>

namespace {
// エディタ用カメラの投影パラメータ（アスペクト比は Renderer の現在値を使う）
constexpr float kEditorCameraFovY = 0.45f;
constexpr float kEditorCameraNear = 0.1f;
constexpr float kEditorCameraFar = 1000.0f;
} // namespace

BaseScene::BaseScene() {
    // 部品へ渡す窓口の束に、カメラの窓口を入れる(メインカメラの設定前から使えるよう、生成のときに 1 回だけ設定する)
    world_.SetScreenView(&cameraView_);
    world_.SetCameraRig(&cameraView_);
    world_.SetPlayState(&playState_);
}

BaseScene::~BaseScene() = default;

void BaseScene::Initialize(const SceneServices &services) {
    services_ = services;
    // 入力は、シーンの開始時に渡される。部品を作る前(読み込みの前)に、束へ設定する
    world_.SetInput(services.input);

    // シーンの開始時の、SRV デスクリプタの使用量(シーンを何周も作り直しても、頭打ちになるかを確かめるため)
    if (auto* dx = Renderer::GetInstance()->GetDX()) {
        const auto& srvAlloc = dx->GetSrvAllocator();
        Logger::GetInstance().Log("[SRV] scene start: id=%s used=%u capacity=%u", sceneId_.c_str(), srvAlloc.UsedCount(), srvAlloc.Capacity());
    }

    // カメラの初期化（ゲーム中も使用するためマクロ外で生成）
    editorCamera_ = std::make_unique<AbsoluteEngine::EditorCamera>();
    editorCamera_->Initialize();
    RefreshEditorCameraProjection();

    // タイムライン機能を使うシーンのみ TimelineManager/TimelineEditorWindow を生成する
    if (UsesTimeline()) {
        timeline_ = std::make_unique<AbsoluteEngine::TimelineHost>();
        // タイムラインのイベントは、世界(SceneWorld)が持つ窓口の束(オブジェクトを出す窓口など)を使う(グローバルな「今のシーン」を探さない)
        timeline_->SetDeps(&world_.Deps());
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
    // エディタ機能の初期化
    editor_ = std::make_unique<AbsoluteEngine::EditorSession>();
    // タイムラインマネージャをインスペクタにポイントする（タスク15）。未使用シーンではnullptrのまま
    editor_->SetTimelineManager(timeline_ ? timeline_->GetManager() : nullptr);
#endif
}

void BaseScene::RemoveRootObject(const std::shared_ptr<AbsoluteEngine::GameObject>& obj) {
  world_.Remove(obj);
}

void BaseScene::RemoveRootObjectsIf(
    const std::function<bool(const std::shared_ptr<AbsoluteEngine::GameObject>&)>& shouldRemove) {
  world_.RemoveIf(shouldRemove);
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
  navigator_.RequestChange(sceneId);
}

void BaseScene::RequestSceneChange(const std::string &sceneId, std::any payload) {
  navigator_.RequestChange(sceneId, std::move(payload));
}

void BaseScene::RequestSceneChangeKeepingPayload(const std::string &sceneId) {
  navigator_.RequestChangeKeepingPayload(sceneId);
}

bool BaseScene::IsSceneTransitioning() const {
  return navigator_.IsTransitioning();
}

float BaseScene::GetTransitionRevealProgress() const {
  return navigator_.GetRevealProgress();
}

float BaseScene::GetTransitionRevealSeconds() const {
  return navigator_.GetRevealSeconds();
}

void BaseScene::BackupScene() {
    world_.SaveSnapshot();
}

void BaseScene::RestoreScene() {
    world_.RestoreSnapshot();

    // タスク12: 復元後にタイムラインのカメラポインタを再接続する
    if (timeline_) {
        for (const auto& obj : world_.Objects()) {
            if (!obj) continue;
            if (auto* railComp = obj->GetComponent<AbsoluteEngine::ITimelineCamera>()) {
                SetTimelineCamera(railComp);
                break;
            }
        }
    }
}

std::string BaseScene::GetSceneFilePath() const {
    const std::string dataName = GetDataName();
    if (dataName.empty()) return AbsoluteEngine::AssetPath::ResolveEditableData("editor/scenes/scene.json");
    return AbsoluteEngine::AssetPath::ResolveEditableData("editor/scenes/" + dataName + ".json");
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
    world_.LoadFromFile(filePath);
    return true;
}

void BaseScene::SaveEditorScene() {
#ifdef USE_IMGUI
    // 保存はエディタビルドのみ。配布ビルドではソース/配布物のJSONを書き換えない
    std::string saveDir = AbsoluteEngine::AssetPath::ResolveEditableData("editor/scenes/");
    std::filesystem::create_directories(saveDir);
    world_.SaveToFile(GetSceneFilePath());
    // タイムラインデータも同時に保存してシーンとタイムラインのデータを完全同期する（タスクB）
    SaveTimeline();
#endif
}

void BaseScene::UpdateWorld() {
#ifdef USE_IMGUI
    // エディタカメラの更新
    // ドラッグ＆ドロップ中はカメラの操作をブロックする。
    // Debug Camera トグルがOFFの派生クラスでは非表示のeditorCamera_が入力を奪わないようにする（タスクD）
    if (editorCamera_ && GetPlayMode() == PlayMode::Edit && !AbsoluteEngine::EditorSession::IsDragDropInProgress() &&
        IsEditorCameraOperationEnabled()) {
        editorCamera_->Update(*services_.input);
    }
#endif

    const float deltaTime = 1.0f / 60.0f; // 固定のdeltaTime

    // タイムラインイベント選択に応じたプレビューオブジェクト管理（タスク16）
    // エディットモード中のみ有効（プレイ中に仮オブジェクトが混入しないように）
    if (GetPlayMode() == PlayMode::Edit && editor_ && timeline_) {
        // 仮のオブジェクトの追加は、実行中の追加と同じ経路(AddRootObject)を通す
        editor_->UpdateTimelinePreview(
            *timeline_, [this](const std::shared_ptr<AbsoluteEngine::GameObject>& obj) { AddRootObject(obj); });
    }

    // プレイモード中のオブジェクトの更新。世界の時間の速さが 0(ヒットストップ)の間は、世界を更新しない
    if (GetPlayMode() == PlayMode::Play && worldTimeScale_ > 0.0f) {
        const float worldDeltaTime = deltaTime * worldTimeScale_; // スローモーションのとき、世界だけが遅くなる
        // タイムラインをオブジェクトより先に更新する
        UpdateTimeline(worldDeltaTime);

        // オブジェクトの更新と衝突判定
        world_.Update(worldDeltaTime);
    }

    // ガベージコレクション（IsActive() == false なオブジェクトを削除）
    // Playモード限定ではなく毎フレーム・常時実行する。GameObject::Destroy()はisActive_を
    // falseにするだけで実際の削除はここで行われるため、Editモード限定（タイムラインの
    // トラック削除やEdit中のタイムラインプレビュー巻き戻しでSpawnEvent::Rewind()がDestroy()
    // する場合など）でもここを通さないと、破棄されたはずのオブジェクトがrootObjects_に
    // 残り続けて描画され続けてしまう（削除したはずのオブジェクトが消えない/再スポーンが
    // 別オブジェクトとして重なって見えるバグの原因だった）
    world_.CollectGarbage();
}

void BaseScene::ApplyToolbarAction(AbsoluteEngine::ToolbarAction action) {
    using AbsoluteEngine::ToolbarAction;
    switch (action) {
    case ToolbarAction::Play:
        BackupScene();
        SetPlayMode(PlayMode::Play);
        // タイムラインもシーンのPlayに合わせて現在位置から再生開始する
        if (timeline_) timeline_->Play();
        break;
    case ToolbarAction::Pause:
        SetPlayMode(PlayMode::Pause);
        // タイムラインも一時停止する
        if (timeline_) timeline_->Pause();
        break;
    case ToolbarAction::Resume:
        SetPlayMode(PlayMode::Play);
        // タイムラインも再生再開する
        if (timeline_) timeline_->Play();
        break;
    case ToolbarAction::Stop:
        RestoreScene();
        if (editor_) editor_->ClearSelection();
        SetPlayMode(PlayMode::Edit);
        // タイムラインを停止・先頭にリセットする
        if (timeline_) timeline_->Stop();
        break;
    case ToolbarAction::None:
        break;
    }
}

void BaseScene::DrawEditorUI() {
#ifdef USE_IMGUI
    if (!editor_) return;

    // ツールバー(押されたボタンに応じて、再生状態を切り替える)
    ApplyToolbarAction(editor_->DrawToolbar(GetPlayMode()));

    if (editorCamera_) {
        // ギズモ・マウスピッキングは「実際に画面に描画されているカメラ」の行列を使う
        // （常にeditorCamera_を使うと、Debug Camera OFF時にGameCamera(レールカメラ)で
        //   描画されている絵とギズモ/クリック判定がズレて、意味不明な視点に見える不具合になる）
        const Camera* viewCam = GetEditorViewCamera();
        if (!viewCam) viewCam = editorCamera_.get();
        editor_->DrawPanels(world_, *viewCam, editorCamera_.get());

        // オートセーブの実行
        // 【バグ修正】ここにplayMode_のガードが無かったため、GameScene（常にPlayModeで
        // 起動）を実際にプレイ中でもマウス操作が偶然ギズモ等に当たって isModified_ が立つと、
        // 「プレイ中に動的生成された敵・弾・ボスを含む今の状態」がそのままシーンJSONへ
        // 上書き保存されてしまっていた（＝二度と正常にロードできない壊れたレベルになる）。
        // 保存はEditモードでの編集結果に対してのみ行う。
        if (GetPlayMode() == PlayMode::Edit && editor_->ConsumeSceneModified()) {
            SaveEditorScene();
        }
    }

    // タイムラインエディタウィンドウの描画
    // （USE_IMGUI ブロック内なのでリリースビルドでも安全）
    DrawTimelineEditorUI();

#endif
}

void BaseScene::ApplyEditorLightsToRenderer(Renderer* renderer) {
    // ライトの収集と Renderer への受け渡しは SceneLights が担当する
    AbsoluteEngine::SceneLights::ApplyToRenderer(world_, renderer);
}

// ============================================================
// タイムライン統合メソッド
// ============================================================

std::string BaseScene::GetTimelineFilePath() const {
    // データ名(既定はシーンID)に "_timeline" を付加したJSONファイルを使用する
    // 例: "devScene" -> "resources/editor/scenes/devScene_timeline.json"
    const std::string dataName = GetDataName();
    const std::string baseName = dataName.empty() ? "scene" : dataName;
    return AbsoluteEngine::AssetPath::ResolveEditableData(
        "editor/scenes/" + baseName + "_timeline.json");
}

void BaseScene::SaveTimeline() {
    // UsesTimeline()がfalseのシーンではtimeline_が存在しないため何もしない
    if (!timeline_) return;
    // タイムラインデータをシーンIDに紐付けたJSONファイルに保存する(保存はエディタビルドのみ。TimelineHost 側で判定)
    timeline_->Save(GetTimelineFilePath());
}

void BaseScene::LoadTimeline() {
    if (!timeline_) return;
    timeline_->Load(GetTimelineFilePath());
}

void BaseScene::SetTimelineCamera(AbsoluteEngine::ITimelineCamera* camera) {
    if (!timeline_) return;
    timeline_->SetCamera(camera);
}

void BaseScene::SetTimelineSpawnAheadDistance(float distance) {
    if (!timeline_) return;
    timeline_->SetSpawnAheadDistance(distance);
}

void BaseScene::SetTimelineHoldAtEnd(bool hold) {
    if (!timeline_) return;
    timeline_->SetHoldAtEnd(hold);
}

bool BaseScene::HasTimelineReachedEnd() const {
    return timeline_ && timeline_->HasReachedEnd();
}

void BaseScene::SeekTimelineToEnd() {
    if (!timeline_) return;
    timeline_->SeekToEnd();
}

void BaseScene::UpdateTimeline(float deltaTime) {
    if (!timeline_) return;
    timeline_->Update(deltaTime);
}

void BaseScene::PlayTimeline() {
    if (!timeline_) return;
    timeline_->Play();
}

void BaseScene::DrawTimelineEditorUI() {
    // UsesTimeline()がfalseのシーンではウィンドウ自体が存在しないため描画しない
    if (!timeline_) return;

    // トラック/イベントの追加・削除等を既存のオートセーブ経路に接続する(エディタUIが無ければ空)
    std::function<void()> onModified;
    if (editor_) {
        AbsoluteEngine::EditorSession* editor = editor_.get();
        onModified = [editor]() { editor->NotifySceneModified(); };
    }

    // Debug/Game カメラの状態は、派生クラスがオーバーライドしていれば渡す(表示用)。
    // ボタンが押されたら、状態の持ち主であるシーンが切り替える
    const std::optional<bool> debugCameraState = GetDebugCameraState();
    const bool debugCameraToggled =
        timeline_->DrawEditorWindow(GetTimelineFilePath(), debugCameraState, std::move(onModified));
    if (debugCameraToggled && debugCameraState.has_value()) {
        SetDebugCameraEnabled(!*debugCameraState);
    }
}
