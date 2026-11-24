#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "TimelineSpawnPreview.h"

namespace AbsoluteEngine {

struct ComponentDeps;
class ITimelineCamera;
class TimelineManager;
class TimelineEditorWindow;

// シーンがタイムライン機能を使うときに、必要な部品(TimelineManager、エディタウィンドウ、プレビュー)を
// まとめて持ち、シーンから使いやすい窓口を提供するクラス。
//
// 分けた理由: BaseScene がタイムラインの部品と操作を全部直接持っていて、タイムラインを使わないシーン
// (タイトルなど)にも関係のない責務が混ざっていたため。タイムラインを使うシーンだけが、このクラスを生成して持つ。
// BaseScene は、使わないシーンでは生成せず(nullptr のまま)、何も呼ばない。
class TimelineHost {
public:
  TimelineHost();
  // unique_ptr で持つ型を、ヘッダで完全な型にしなくて済むよう、定義は .cpp に置く
  ~TimelineHost();

  // --- 設定 ---
  // タイムラインが進行度を同期するカメラを設定する(所有しない)
  void SetCamera(ITimelineCamera *camera);
  // イベントがオブジェクトを出すときなどに使う窓口の束を設定する(所有しない)
  void SetDeps(const ComponentDeps *deps);
  // SpawnEvent の自動配置で、カメラ前方へ置く距離を設定する
  void SetSpawnAheadDistance(float distance);
  // 終端に着いたとき、先頭へ戻らず、終端で止めて保つかを設定する
  void SetHoldAtEnd(bool hold);

  // --- 再生制御 ---
  // 時間を進めて、イベントを評価する(プレイモード中に毎フレーム呼ぶ)
  void Update(float deltaTime);
  // 現在位置から再生を始める
  void Play();
  // 一時停止する
  void Pause();
  // 停止して、先頭へ戻す
  void Stop();
  // 終端に到達して、SetHoldAtEnd(true) の動きで止まったか
  bool HasReachedEnd() const;
  // 終端の時刻へ飛ぶ(デバッグ用)
  void SeekToEnd();

  // --- ファイル ---
  // JSON ファイルから読み込み、先頭の状態にそろえる。
  // エディタビルドでは、ファイルが無ければ空のまま。配布ビルドでは、ファイルが無いと止める
  void Load(const std::string &filePath);
  // JSON ファイルへ保存する(エディタビルドのみ。配布ビルドでは何もしない)
  void Save(const std::string &filePath) const;

  // --- エディタ ---
  // タイムラインのエディタウィンドウを描画する。
  // timelineFilePath: ウィンドウ内の保存ボタンが使うファイルパス
  // debugCameraState: いまのデバッグカメラの状態(true = 使用中)。ボタンの見た目に使う。nullopt ならトグルを出さない
  // onModified: データが変更されたときに呼ぶ関数(オートセーブ用。空でもよい)
  // 戻り値: Debug/Game カメラの切り替えボタンが押されたか(切り替えは、呼び出し側が行う)
  bool DrawEditorWindow(const std::string &timelineFilePath, std::optional<bool> debugCameraState,
                        std::function<void()> onModified);
  // エディットモード中に毎フレーム呼ぶ。選択したイベントの、仮の敵(プレビュー)の出し入れと書き戻しを行う
  void UpdatePreview(const TimelineSpawnPreview::AddObjectFunc &addObject,
                     const TimelineSpawnPreview::SelectObjectFunc &selectObject);

  // エディタのインスペクタが、タイムラインのイベントを編集するために必要な、タイムライン本体への窓口。
  // 所有権は渡さない。保持せず、使うたびに取得すること(このクラスの破棄後は無効になる)
  TimelineManager *GetManager() { return manager_.get(); }

private:
  // 宣言順は初期化・破棄の順序を決める。editorWindow_ は manager_ を参照するため、manager_ より後に宣言する
  std::unique_ptr<TimelineManager> manager_;
  std::unique_ptr<TimelineEditorWindow> editorWindow_;
  TimelineSpawnPreview preview_;
};

} // namespace AbsoluteEngine
