#pragma once
#include <memory>

#include "PlayMode.h"
#include "TimelineSpawnPreview.h"

class Camera;

namespace AbsoluteEngine {

class CommandManager;
class EditorCamera;
class EditorUIManager;
class SceneWorld;
class TimelineHost;
class TimelineManager;

// ツールバーで押されたボタン。UI は「何が押されたか」だけを返し、実際の処理は再生状態を持つ側(BaseScene)が行う。
enum class ToolbarAction {
  None,   // 何も押されていない
  Play,   // 編集モードから再生を始める
  Pause,  // 再生を一時停止する
  Resume, // 一時停止から再生を再開する
  Stop    // 停止して、再生前の状態へ戻す
};

// エディタのときだけ存在する UI と操作(ツールバー、編集パネル、選択、オートセーブの要求)をまとめたクラス。
//
// 分けた理由: BaseScene が、ゲーム本体の骨格と、エディタ専用の UI を両方持っていて責務が混ざっていたため。
// このクラスはゲームの状態(PlayMode やシーンの中身)を書き換えない。
// 再生状態は引数で受け取り、押されたボタンを返すだけにして、状態の変更は BaseScene に任せる。
// 配布ビルド(USE_IMGUI なし)では、BaseScene がこのクラスを生成しない。
class EditorSession {
public:
  EditorSession();
  // unique_ptr で持つ型を、ヘッダで完全な型にしなくて済むよう、定義は .cpp に置く
  ~EditorSession();

  // インスペクタが、タイムラインのイベントを編集するためのタイムライン本体を設定する(所有しない)。使わないシーンは nullptr
  void SetTimelineManager(TimelineManager *manager);

  // ツールバーのウィンドウを描画し、押されたボタンを返す。mode: 現在の再生状態(押せるボタンが変わる)
  ToolbarAction DrawToolbar(PlayMode mode);

  // 編集パネル一式(ヒエラルキー、インスペクタ、ビューポートなど)を描画する。
  // ビューポートの描画はプレイ中も必要なため、プレイ中も呼ぶ。
  // viewCamera: 実際に画面へ描画しているカメラ(ギズモとクリック判定は、この行列を使う)
  // editorCamera: ビューポート操作から位置を求めるためのカメラ
  void DrawPanels(SceneWorld &world, const Camera &viewCamera, EditorCamera *editorCamera);

  // 変更があったか(オートセーブが必要か)を取得して、フラグを下ろす
  bool ConsumeSceneModified();
  // シーンが変更されたことを通知する(タイムライン側の編集など、外から変更を知らせるとき用)
  void NotifySceneModified();

  // 選択中のオブジェクトを解除する(再生を止めて、シーンを復元した直後など、古い参照を残さないため)
  void ClearSelection();

  // タイムラインで選んだ SpawnEvent の、仮の敵(プレビュー)を更新する。エディットモード中に毎フレーム呼ぶ。
  // addObject: 仮のオブジェクトをシーンへ追加する関数(派生シーンの追加処理を通すため、呼び出し側から受け取る)
  void UpdateTimelinePreview(TimelineHost &timeline, const TimelineSpawnPreview::AddObjectFunc &addObject);

  // ImGui でドラッグ&ドロップの最中か(カメラの操作を止めるために使う。USE_IMGUI が無ければ false)
  static bool IsDragDropInProgress();

  // 元に戻す・やり直しのコマンド管理(読み取り専用の窓口。所有権は渡さない)
  CommandManager *GetCommandManager() const;

private:
  std::unique_ptr<EditorUIManager> ui_;
};

} // namespace AbsoluteEngine
