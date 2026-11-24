#pragma once
#include <any>
#include <string>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "ISpawner.h"
#include "MainCameraView.h"
#include "PlayModeState.h"
#include "PlayMode.h"
#include "SceneNavigator.h"
#include "SceneServices.h"
#include "SceneWorld.h"
#include "Vector.h"
#include "TimelineHost.h"

class Camera;
class GameCamera;
namespace AbsoluteEngine {
    class GameObject;
    class EditorSession;
    enum class ToolbarAction;
    class EditorCamera;
    class CommandManager;
    class ITimelineCamera;
}

class BaseScene {
public:
  // コンストラクタとデストラクタは、不完全な型(EditorSession)の unique_ptr を持つため、定義を .cpp に置く
  BaseScene();
  virtual ~BaseScene();

  // シーン切り替えの窓口(SceneManager)を受け取る。SceneManager が、シーンを作った後に呼ぶ
  void SetSceneChanger(ISceneChanger *changer) { navigator_.SetChanger(changer); }
  void SetSceneId(const std::string& id) { sceneId_ = id; }
  // 前のシーンから渡された荷物を受け取る。SceneManager が、シーンを作った直後、Initialize の前に呼ぶ。
  // 荷物なしの切り替えでは、空の std::any が渡る
  void SetLaunchPayload(std::any payload) { navigator_.SetPayload(std::move(payload)); }

  virtual void Initialize(const SceneServices &services);
  virtual void Finalize() {}
  virtual void Update() {}
  virtual void Draw() = 0;
  virtual void DrawEditorUI();

  PlayMode GetPlayMode() const { return playState_.GetPlayMode(); }
  void SetPlayMode(PlayMode mode) { playState_.SetPlayMode(mode); }

  // 世界(タイムライン、オブジェクトの更新、衝突判定)の時間の速さ。1 で通常、0 で停止(その間、世界の更新は行わない)、
  // 0 より大きく 1 より小さいとスローモーション。死亡演出のヒットストップとスローモーションに使う。
  // プレイ中の更新にだけ効く(エディットモードのタイムラインのプレビューには掛からない)。0 未満は 0 にそろえる
  void SetWorldTimeScale(float scale) { worldTimeScale_ = scale < 0.0f ? 0.0f : scale; }
  float GetWorldTimeScale() const { return worldTimeScale_; }

  const std::vector<std::shared_ptr<AbsoluteEngine::GameObject>>& GetRootObjects() const { return world_.Objects(); }
  // 実行中のオブジェクト追加(弾、敵など)。SceneWorld::Spawn を通るので、SetObjectSpawnedCallback で登録された通知も働く。
  // ゲーム固有の追加処理は、派生クラスが上書きするのではなく、通知として登録する
  void AddRootObject(std::shared_ptr<AbsoluteEngine::GameObject> obj) { world_.Spawn(std::move(obj)); }
  // オブジェクトを出す窓口(ISpawner)を返す。イベントや部品へ、シーン全体ではなく「出す機能」だけを渡すために使う。
  // 保持する場合は、このシーンが生きている間だけ有効
  AbsoluteEngine::ISpawner &GetSpawner() { return world_; }
  // 部品へ渡す窓口の束を返す。プレハブの読み込みなど、部品を作る場面へ渡すために使う。このシーンが生きている間だけ有効
  const AbsoluteEngine::ComponentDeps &GetComponentDeps() const { return world_.Deps(); }
  // 画面(メインカメラ)の見え方を知る窓口(読み取り専用)を返す。部品や補助クラスへ、カメラの型を見せずに渡すために使う。
  // メインカメラの設定や差し替えに追従する。このシーンが生きている間だけ有効
  AbsoluteEngine::IScreenView &GetScreenView() { return cameraView_; }
  // シーンのオブジェクトの一覧を、読み取り専用で調べる窓口を返す。部品や補助クラスへ、シーンを見せずに渡すために使う。このシーンが生きている間だけ有効
  AbsoluteEngine::ISceneQuery &GetSceneQuery() { return world_; }
  // 再生状態(エディットか、プレイか)を、読み取り専用で知る窓口を返す。このシーンが生きている間だけ有効
  AbsoluteEngine::IPlayState &GetPlayState() { return playState_; }
  // メインカメラの位置と向きを書き換える窓口を返す。レールカメラの部品へ渡すために使う。このシーンが生きている間だけ有効
  AbsoluteEngine::ICameraRig &GetCameraRig() { return cameraView_; }
  void ClearRootObjects() { world_.Clear(); }
  // 指定したオブジェクトをシーンから取り除く（含まれていなければ何もしない）
  // GetRootObjects() の巡回中は呼ばないこと（巡回中に削除するとイテレータが無効になる）。
  // 巡回中に消したいものは、いったん集めておき、巡回の後で呼ぶ
  void RemoveRootObject(const std::shared_ptr<AbsoluteEngine::GameObject>& obj);
  // 条件に合うオブジェクトをまとめて取り除く。shouldRemove は各オブジェクトに対して1回ずつ呼ばれる
  // 条件の中でシーンのオブジェクト一覧を変更しないこと
  void RemoveRootObjectsIf(const std::function<bool(const std::shared_ptr<AbsoluteEngine::GameObject>&)>& shouldRemove);

  // エンジンインフラ機能のグローバルアクセス
  GameCamera* GetMainCamera() const { return mainCamera_.get(); }
  void SetMainCamera(std::shared_ptr<GameCamera> camera) { mainCamera_ = camera; }
  class Input* GetInput() const { return services_.input; }

protected:
  // オブジェクトが AddRootObject(ISpawner)で追加された直後に呼ぶ関数を登録する。
  // 敵へ撃破時のコールバックを付けるなど、シーン固有の追加処理に使う。派生クラスの Initialize で、最初のオブジェクトを出す前に呼ぶ
  void SetObjectSpawnedCallback(AbsoluteEngine::SceneWorld::SpawnedCallback callback) { world_.SetOnSpawned(std::move(callback)); }

  virtual void BackupScene();
  virtual void RestoreScene();
  virtual void SaveEditorScene();
  bool LoadEditorScene();
  std::string GetSceneFilePath() const;

  // タイムラインデータのファイルパスを取得する（GetDataName() + "_timeline.json"）
  std::string GetTimelineFilePath() const;

  // シーンJSON と タイムラインJSON のファイル名の元になる名前(拡張子なし)。
  // 既定はシーンID。同じシーンのクラスが、荷物の内容によって別のデータを読むとき(ステージごとのデータなど)は、
  // 派生クラスでオーバーライドして、荷物から決めた名前を返す。
  // 荷物は Initialize より前に入っているので、Initialize の中の読み込みから使ってよい
  virtual std::string GetDataName() const { return sceneId_; }

  // タイムラインデータを保存・読み込みする
  void SaveTimeline();
  void LoadTimeline();

  // タイムラインのカメラを設定する（ITimelineCameraを実装するコンポーネントへの参照を渡す）
  // 派生クラスのInitializeでカメラコンポーネントを設定後に呼ぶ
  void SetTimelineCamera(AbsoluteEngine::ITimelineCamera* camera);

  // SpawnEvent自動配置(カメラ前方へのオフセット)の距離を設定する。
  // 既定値はエンジン側の暫定値なので、ゲームのプレイヤー/照準距離に合わせて派生クラスから呼ぶ
  void SetTimelineSpawnAheadDistance(float distance);

  // タイムラインが終端に着いたとき、先頭へ戻らず、終端で止めて保つかを設定する(既定は戻る)。
  // 保つと、カメラの進行度が 1.0 のまま残る。ゲームのボス戦など、終端の先でレールを再走させたくないときに使う
  void SetTimelineHoldAtEnd(bool hold);

  // タイムラインが終端に到達して止まったか(SetTimelineHoldAtEnd(true) のとき)。
  // 「終点に到達した」という明示の合図。タイムラインが無いシーンでは常に false
  bool HasTimelineReachedEnd() const;
  // タイムラインを終端の時刻へジャンプさせる(デバッグ用。ボス戦まで飛ばすのに使う)。
  // 途中のイベントは、1 回に発火する。SetTimelineHoldAtEnd(true) のときは、次の更新で終端へ着いた扱いになり、出した敵は巻き戻して片づく
  void SeekTimelineToEnd();

  // タイムラインを更新する（プレイモード中に毎フレーム呼ぶ）
  void UpdateTimeline(float deltaTime);

  // タイムラインの再生を開始する（タイムラインを使わないシーンでは何もしない）
  void PlayTimeline();

  // タイムラインのエディタUIを描画する
  void DrawTimelineEditorUI();

protected:
  void RequestSceneChange(const std::string &sceneId);
  // 次のシーンへ渡す荷物つきで、シーンの切り替えを要求する。荷物の型はコピーできること。
  // 受け取るシーンは、TryGetLaunchPayload<同じ型>() で取り出す
  void RequestSceneChange(const std::string &sceneId, std::any payload);
  // このシーンが受け取った荷物を、そのまま次のシーンへ引き継いで、切り替えを要求する。
  // 荷物を受け取っていなければ、荷物なしの切り替えと同じ。「同じ条件でもう一度」や「前の画面へ戻る」のように、
  // 荷物の中身を知らずに(型を取り出さずに)引き継ぎたいときに使う
  void RequestSceneChangeKeepingPayload(const std::string &sceneId);

  // 前のシーンから渡された荷物を、型 T で取り出す。荷物が無い、または型が違うときは nullptr(その場合は既定の動作にする)。
  // 返す参照は、このシーンが生きている間だけ有効
  template <typename T>
  const T *TryGetLaunchPayload() const {
    return navigator_.TryGetPayload<T>();
  }

  // 画面遷移の進行状況を問い合わせる（読み取り専用）。SceneManager 自体は派生クラスに公開しない
  // 遷移の進行中か（SceneManager が未設定の場合は false）
  bool IsSceneTransitioning() const;
  // 覆いが晴れる途中の進み具合(0-1)。遷移中でない、または SceneManager が未設定なら 1
  float GetTransitionRevealProgress() const;
  // 覆いが晴れる区間の長さ（秒）。SceneManager が未設定なら 0
  float GetTransitionRevealSeconds() const;

  // 世界の更新(タイムライン、オブジェクト、衝突判定、不要物の回収)と、エディタ入力の更新。
  // 派生クラスのUpdateから毎フレーム呼ばれることを想定している
  virtual void UpdateWorld();
  void ApplyEditorLightsToRenderer(class Renderer* renderer);

  // editorCamera_（Debug/Free Camera）による入力操作を有効にするかどうか（タスクD）
  // GameScene 側の「Debug Camera」トグルと描画分岐を一致させるため、
  // 派生クラスでオーバーライドしてトグル状態を返すことで、
  // OFF時は非表示のeditorCamera_が入力を奪わないようにする
  virtual bool IsEditorCameraOperationEnabled() const { return true; }

  // デバッグカメラ(Free Camera)の、いまの状態を返す（Timeline Editorのトグルボタンの表示用）。
  // 持つシーン(GameScene等)は、オーバーライドして、使用中なら true を返す。
  // 既定は std::nullopt(デバッグカメラを持たない)で、その場合 Timeline Editor はトグルボタンを表示しない
  virtual std::optional<bool> GetDebugCameraState() const { return std::nullopt; }
  // Timeline Editor のボタンが押されたときに呼ばれる。デバッグカメラを持つシーンは、オーバーライドして状態を切り替える。
  // 状態の持ち主はシーンで、Timeline Editor は直接書き換えない
  virtual void SetDebugCameraEnabled(bool /*enabled*/) {}

  // このシーンがタイムライン機能(TimelineManager/TimelineEditorWindow、および
  // シーンごとの "<sceneId>_timeline.json" の自動保存/読み込み)を使うかどうか。
  // レール演出を持たないシーン（タイトル/クリア/ゲームオーバー等）や、
  // タイムラインという概念自体を持たない別ジャンルのゲームではfalseにオーバーライドする。
  // falseの場合、timeline_(TimelineHost)はInitialize()で生成されず、
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
  // 宣言順は初期化・破棄の順序を決めるため、依存関係（例: editor_ は world_ より先に宣言し、後に破棄する）を
  // 崩さないよう、順序を変えないこと
private:
  // ツールバーで押されたボタンに応じて、再生状態・バックアップと復元・タイムラインの再生を切り替える
  void ApplyToolbarAction(AbsoluteEngine::ToolbarAction action);

  SceneServices services_{}; // 派生クラスからは GetInput() 経由で扱う

  // エンジン・エディタ機能
  // エディタのときだけ存在する UI と操作。配布ビルド(USE_IMGUI なし)では生成しない(nullptr のまま)
  std::unique_ptr<AbsoluteEngine::EditorSession> editor_;
  // シーン直下のオブジェクトの管理(追加・削除・更新・回収・保存と復元)。派生クラスからは GetRootObjects()（読み取り）、
  // AddRootObject（追加）、RemoveRootObject / RemoveRootObjectsIf（削除）経由で扱う
  AbsoluteEngine::SceneWorld world_;
  // エディタ用カメラ（常に EditorCamera として生成する）。派生クラスからは GetEditorCamera() などの専用メソッド経由で扱う
  std::unique_ptr<AbsoluteEngine::EditorCamera> editorCamera_;
  std::shared_ptr<GameCamera> mainCamera_; // ゲーム全体で共有するメインカメラ
  // メインカメラを、IScreenView / ICameraRig の窓口として見せる。mainCamera_ への参照を持つため、mainCamera_ より後に宣言する
  AbsoluteEngine::MainCameraView cameraView_{mainCamera_};

  AbsoluteEngine::PlayModeState playState_; // 再生状態。派生クラスからは GetPlayMode/SetPlayMode 経由で扱う
  float worldTimeScale_ = 1.0f;        // 世界の時間の速さ(SetWorldTimeScale)
  std::string sceneId_ = "";
  // シーン切り替えの要求と、前のシーンから渡された荷物(派生クラスからは RequestSceneChange / TryGetLaunchPayload 経由で扱う)
  SceneNavigator navigator_;

  // タイムライン機能(マネージャ、エディタウィンドウ、プレビュー)のまとまり。
  // UsesTimeline()がfalseのシーンでは生成されない（nullptrのまま）
  // 派生クラスからは SetTimelineCamera/UpdateTimeline/PlayTimeline など専用メソッド経由で扱う
  std::unique_ptr<AbsoluteEngine::TimelineHost> timeline_;
};
