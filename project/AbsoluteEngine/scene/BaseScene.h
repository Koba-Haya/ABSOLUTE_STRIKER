#pragma once
#include <string>
#include <functional>
#include <memory>
#include <vector>

#include "SceneServices.h"
#include "Vector.h"
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

  // 世界(タイムライン、オブジェクトの更新、衝突判定)の時間の速さ。1 で通常、0 で停止(その間、世界の更新は行わない)、
  // 0 より大きく 1 より小さいとスローモーション。死亡演出のヒットストップとスローモーションに使う。
  // プレイ中の更新にだけ効く(エディットモードのタイムラインのプレビューには掛からない)。0 未満は 0 にそろえる
  void SetWorldTimeScale(float scale) { worldTimeScale_ = scale < 0.0f ? 0.0f : scale; }
  float GetWorldTimeScale() const { return worldTimeScale_; }

  const std::vector<std::shared_ptr<AbsoluteEngine::GameObject>>& GetRootObjects() const { return rootObjects_; }
  // タイムラインのSpawnEventなど、あらゆる箇所からのオブジェクト追加を一元管理できるよう virtual 化する
  // 派生クラスでオーバーライドすることで、Application固有の処理（コールバック注入など）を追加できる（DI）
  virtual void AddRootObject(std::shared_ptr<AbsoluteEngine::GameObject> obj) { rootObjects_.push_back(obj); }
  void ClearRootObjects() { rootObjects_.clear(); }
  // 指定したオブジェクトをシーンから取り除く（含まれていなければ何もしない）
  // GetRootObjects() の巡回中は呼ばないこと（巡回中に削除するとイテレータが無効になる）。
  // 巡回中に消したいものは、いったん集めておき、巡回の後で呼ぶ
  void RemoveRootObject(const std::shared_ptr<AbsoluteEngine::GameObject>& obj);
  // 条件に合うオブジェクトをまとめて取り除く。shouldRemove は各オブジェクトに対して1回ずつ呼ばれる
  // 条件の中でシーンのオブジェクト一覧を変更しないこと
  void RemoveRootObjectsIf(const std::function<bool(const std::shared_ptr<AbsoluteEngine::GameObject>&)>& shouldRemove);

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
  // 既定は editorCamera_（EditorCamera の完全な型が必要なため、実装は .cpp に置く）
  // 行列の取得にしか使わないため const で返す
  virtual const Camera* GetEditorViewCamera() const;

  // エディタ用カメラを読み取り専用で取得する（行列の取得、描画への受け渡しなど）
  // 生成前（Initialize 前）は nullptr。保持せず、使うたびに取得すること（シーン破棄後は無効になる）
  const AbsoluteEngine::EditorCamera* GetEditorCamera() const { return editorCamera_.get(); }

  // エディタ用カメラの投影を、定数の画角・クリップ面と「現在の画面のアスペクト比」で作り直す
  // Initialize で呼ばれる。画面のアスペクト比が変わる場合は、その後にも呼んで投影を更新する
  void RefreshEditorCameraProjection();

  // エディタ用カメラの位置・回転の入力欄を、呼び出し側の ImGui ウィンドウの中に描画する
  // カメラが未生成なら何も描かず false を返す（呼び出し側が代替表示を出せるようにする）
  bool DrawEditorCameraControls();

  // エディタ用カメラにシェイク（位置のずれ）を与え、行列へ反映する。ずれを解除するときは {0,0,0} を渡す
  // 毎フレーム、描画に渡す前に呼ぶ（SetShakeOffset と UpdateMatrix の呼び忘れ・順序間違いを防ぐ）
  void SetEditorCameraShake(const Vector3 &offset);

  // メンバ変数は全て private。派生クラスには、上の専用メソッド（GetInput、GetRootObjects など）だけを公開する。
  // 宣言順は初期化・破棄の順序を決めるため、依存関係（例: timelineEditorWindow_ は timelineManager_ を参照する）を
  // 崩さないよう、順序を変えないこと
private:
  SceneManager *sceneManager_ = nullptr;
  SceneServices services_{}; // 派生クラスからは GetInput() 経由で扱う

  // エンジン・エディタ機能
  std::unique_ptr<AbsoluteEngine::EditorUIManager> editorUIManager_;
  // シーン直下のオブジェクト一覧。派生クラスからは GetRootObjects()（読み取り）、AddRootObject（追加）、
  // RemoveRootObject / RemoveRootObjectsIf（削除）経由で扱う
  std::vector<std::shared_ptr<AbsoluteEngine::GameObject>> rootObjects_;
  // エディタ用カメラ（常に EditorCamera として生成する）。派生クラスからは GetEditorCamera() などの専用メソッド経由で扱う
  std::unique_ptr<AbsoluteEngine::EditorCamera> editorCamera_;
  std::shared_ptr<GameCamera> mainCamera_; // ゲーム全体で共有するメインカメラ

  PlayMode playMode_ = PlayMode::Edit; // 派生クラスからは GetPlayMode/SetPlayMode 経由で扱う
  float worldTimeScale_ = 1.0f;        // 世界の時間の速さ(SetWorldTimeScale)
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
