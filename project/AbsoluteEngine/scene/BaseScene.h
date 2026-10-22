#pragma once
#include <string>
#include <memory>
#include <vector>

#include "SceneServices.h"
#include "timeline/TimelineManager.h"
#include "timeline/TimelineEditorWindow.h"

class Camera;
class GameCamera;
namespace AbsoluteEngine {
    class GameObject;
    class EditorUIManager;
    class EditorCamera;
    class CommandManager;
    class ITimelineCamera;
}

class SceneManager;

enum class PlayMode {
  Edit,
  Play,
  Pause
};

class BaseScene {
public:
  virtual ~BaseScene();

  void SetSceneManager(SceneManager *sm) { sceneManager_ = sm; }
  void SetSceneId(const std::string& id) { sceneId_ = id; }

  virtual void Initialize(const SceneServices &services);
  virtual void Finalize() {}
  virtual void Update() {}
  virtual void Draw() = 0;
  virtual void DrawEditorUI();

  PlayMode GetPlayMode() const { return playMode_; }
  void SetPlayMode(PlayMode mode) { playMode_ = mode; }

  const std::vector<std::shared_ptr<AbsoluteEngine::GameObject>>& GetRootObjects() const { return rootObjects_; }
  // タイムラインのSpawnEventなど、あらゆる箇所からのオブジェクト追加を一元管理できるよう virtual 化する
  // 派生クラスでオーバーライドすることで、Application固有の処理（コールバック注入など）を追加できる（DI）
  virtual void AddRootObject(std::shared_ptr<AbsoluteEngine::GameObject> obj) { rootObjects_.push_back(obj); }
  void ClearRootObjects() { rootObjects_.clear(); }

  // アクティブシーンへのグローバルアクセス
  static BaseScene* GetActiveScene() { return activeScene_; }

  AbsoluteEngine::CommandManager* GetCommandManager() const;

  // エンジンインフラ機能のグローバルアクセス
  GameCamera* GetMainCamera() const { return mainCamera_.get(); }
  void SetMainCamera(std::shared_ptr<GameCamera> camera) { mainCamera_ = camera; }
  class Input* GetInput() const { return services_.input; }

protected:
  virtual void BackupScene();
  virtual void RestoreScene();
  virtual void SaveEditorScene();
  bool LoadEditorScene();
  std::string GetSceneFilePath() const;

  // タイムラインデータのファイルパスを取得する（sceneId_ + "_timeline.json"）
  std::string GetTimelineFilePath() const;

  // タイムラインデータを保存・読み込みする
  void SaveTimeline();
  void LoadTimeline();

  // タイムラインのカメラを設定する（ITimelineCameraを実装するコンポーネントへの参照を渡す）
  // 派生クラスのInitializeでカメラコンポーネントを設定後に呼ぶ
  void SetTimelineCamera(AbsoluteEngine::ITimelineCamera* camera);

  // SpawnEvent自動配置(カメラ前方へのオフセット)の距離を設定する。
  // 既定値はエンジン側の暫定値なので、ゲームのプレイヤー/照準距離に合わせて派生クラスから呼ぶ
  void SetTimelineSpawnAheadDistance(float distance);

  // タイムラインを更新する（プレイモード中に毎フレーム呼ぶ）
  void UpdateTimeline(float deltaTime);

  // タイムラインの再生を開始する（タイムラインを使わないシーンでは何もしない）
  void PlayTimeline();

  // タイムラインのエディタUIを描画する
  void DrawTimelineEditorUI();

protected:
  void RequestSceneChange(const std::string &sceneId);

  // 画面遷移の進行状況を問い合わせる（読み取り専用）。SceneManager 自体は派生クラスに公開しない
  // 遷移の進行中か（SceneManager が未設定の場合は false）
  bool IsSceneTransitioning() const;
  // 覆いが晴れる途中の進み具合(0-1)。遷移中でない、または SceneManager が未設定なら 1
  float GetTransitionRevealProgress() const;
  // 覆いが晴れる区間の長さ（秒）。SceneManager が未設定なら 0
  float GetTransitionRevealSeconds() const;

  // エディタ機能の更新・描画（派生クラスのUpdate/Drawから呼ばれることを想定）
  virtual void UpdateEditor();
  void ApplyEditorLightsToRenderer(class Renderer* renderer);

  // editorCamera_（Debug/Free Camera）による入力操作を有効にするかどうか（タスクD）
  // GameScene 側の「Debug Camera」トグルと描画分岐を一致させるため、
  // 派生クラスでオーバーライドしてトグル状態を返すことで、
  // OFF時は非表示のeditorCamera_が入力を奪わないようにする
  virtual bool IsEditorCameraOperationEnabled() const { return true; }

  // Debug/Game カメラ切替フラグへの非所有ポインタを返す（Timeline Editorのトグルボタン用）
  // 派生クラス（GameScene等）がisDebugCamera_のアドレスを返すようオーバーライドする
  // デフォルトはnullptrで、その場合Timeline Editorはトグルボタンを表示しない
  virtual bool* GetDebugCameraFlag() { return nullptr; }

  // このシーンがタイムライン機能(TimelineManager/TimelineEditorWindow、および
  // シーンごとの "<sceneId>_timeline.json" の自動保存/読み込み)を使うかどうか。
  // レール演出を持たないシーン（タイトル/クリア/ゲームオーバー等）や、
  // タイムラインという概念自体を持たない別ジャンルのゲームではfalseにオーバーライドする。
  // falseの場合、timelineManager_/timelineEditorWindow_はInitialize()で生成されず、
  // Timeline EditorパネルもJSON書き出しも一切発生しない。
  virtual bool UsesTimeline() const { return true; }

  // エディタのギズモ操作・マウスピッキングに使う view/projection 行列を提供するカメラを返す
  // 派生クラス（GameScene等）は実際に画面に描画しているカメラ（Draw()内の分岐）と
  // 必ず一致させるようオーバーライドすること。ここが描画カメラとズレると、
  // ギズモやクリック判定が画面上の見た目と食い違う不具合になる
  virtual Camera* GetEditorViewCamera() const { return editorCamera_.get(); }

  // メンバ変数の宣言順は初期化・破棄の順序を決めるため、アクセス指定子を挟みながら元の順を保つ。
  // 派生シーンが直接使うものだけ protected に残し、BaseScene 内部だけで使うものは private にする
private:
  SceneManager *sceneManager_ = nullptr;
  SceneServices services_{}; // 派生クラスからは GetInput() 経由で扱う

  // エンジン・エディタ機能
  std::unique_ptr<AbsoluteEngine::EditorUIManager> editorUIManager_;
protected:
  std::vector<std::shared_ptr<AbsoluteEngine::GameObject>> rootObjects_;
  std::unique_ptr<Camera> editorCamera_; // エディタ用カメラ
private:
  std::shared_ptr<GameCamera> mainCamera_; // ゲーム全体で共有するメインカメラ

  PlayMode playMode_ = PlayMode::Edit; // 派生クラスからは GetPlayMode/SetPlayMode 経由で扱う
  std::string sceneId_ = "";
  std::string backupSceneJson_ = "";

  // タイムラインシステム（シーン独立のため BaseScene が所有する）
  // UsesTimeline()がfalseのシーンでは生成されない（nullptrのまま）
  // 派生クラスからは SetTimelineCamera/UpdateTimeline/PlayTimeline など専用メソッド経由で扱う
  std::unique_ptr<AbsoluteEngine::TimelineManager> timelineManager_;
  std::unique_ptr<AbsoluteEngine::TimelineEditorWindow> timelineEditorWindow_;

  // タイムラインプレビューオブジェクト（タスク16）
  // イベント選択時に生成した仮のオブジェクトへの弱参照
  // 他のイベントへ選択が切り替わった際は古いプレビューを Destroy()してから新規生成する
  std::weak_ptr<AbsoluteEngine::GameObject> timelinePreviewObject_;

  // 前フレームの選択イベントポインタ（選択切り替え検出用）を保持する
  AbsoluteEngine::ITimelineEvent* prevSelectedTimelineEvent_ = nullptr;

  static BaseScene* activeScene_;
};
