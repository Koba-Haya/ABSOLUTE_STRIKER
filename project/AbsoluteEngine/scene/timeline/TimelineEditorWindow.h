// ============================================================
// TimelineEditorWindow.h
// 役割: ImGuiを使ってタイムラインを可視化・編集するエディタウィンドウ
//       コアロジック（TimelineManager）とUIの分離を徹底する
// ============================================================
#pragma once
#include "TimelineManager.h"
#include "ITimelineEvent.h"
#include <string>
#include <functional>
#include <memory>
#include <optional>

// 前方宣言（CommandManagerの完全定義は.cppでのみインクルードする）
namespace AbsoluteEngine { class CommandManager; }

namespace AbsoluteEngine {

class TimelineEditorWindow {
public:
    TimelineEditorWindow() = default;
    ~TimelineEditorWindow() = default;

    // TimelineManager への参照を設定する
    // TimelineEditorWindowはManagerの寿命を管理しない（非所有参照）
    void SetManager(TimelineManager* manager) { manager_ = manager; }

    // CommandManager への参照を設定する（Undo/Redo用: タスクF）
    // nullptr を渡した場合はコマンド発行を無効化する
    void SetCommandManager(CommandManager* commandManager) { commandManager_ = commandManager; }

    // Debug/Game カメラ切替ボタンの表示用に、いまのデバッグカメラの状態(true = 使用中)を伝える。
    // 状態そのものは、このウィンドウが書き換えない(持ち主は、シーン側)。
    // std::nullopt の場合は、デバッグカメラを持たないシーンなので、トグルボタンを表示しない
    void SetDebugCameraState(std::optional<bool> state) { debugCameraState_ = state; }

    // トラック/イベントの追加・削除等でシーンが変更された際に呼ばれるコールバックを設定する
    // 既存のオートセーブ経路（EditorUIManager::SetSceneModified）に接続するために使う
    void SetOnModifiedCallback(std::function<void()> callback) { onModified_ = std::move(callback); }

    // 毎フレームUIを描画する（BaseScene::DrawEditorUI から呼ばれる想定）
    // USE_IMGUI マクロが無効な場合は何もしない。
    // 戻り値: このフレームに、Debug Camera の切り替えボタンが押されたか(押されたら true。切り替えは呼び出し側が行う)
    bool Draw(const std::string& timelineFilePath);

private:
#ifdef USE_IMGUI
    // ツールバー（1行目: 再生系+Debug Cameraトグル、2行目: プレハブ関連）を描画する。
    // 戻り値: Debug Camera の切り替えボタンが押されたか
    bool DrawToolbar(const std::string& timelineFilePath);

    // ルーラー（目盛り+再生ヘッド）とシークスライダーを描画する
    // トラックレーンと同じ座標系（ラベル列オフセット+可変レーン幅）を共有する
    void DrawSeekBar();

    // トラックとイベントのレーンを描画する
    void DrawTracks();

    // 1つのトラックのイベントレーンを描画する（インデックスとレーン幅を指定）
    void DrawTrackLane(size_t trackIndex, float laneWidth);

    // イベント追加ポップアップを描画する。TimelineEventFactoryに登録されている
    // 全イベント種別から選べる（エンジンはSpawnEvent以外の型名を一切知らなくてよい）。
    // 選択した型は TimelineEventFactory::Create() で生成し、型固有の入力欄は
    // ITimelineEvent::DrawCreationUI() をポリモーフィックに呼んで描画する
    void DrawAddEventPopup();

    // トラックインデックスに応じた固定パレット色を返す（重なり識別用、戻り値はImU32と同一表現）
    static unsigned int GetTrackColor(size_t trackIndex);
#endif

private:
    // TimelineManager への非所有ポインタ
    TimelineManager* manager_ = nullptr;

    // CommandManager への非所有ポインタ（Undo/Redo用: タスクF）
    // nullptr の場合はコマンド発行を行わない
    CommandManager* commandManager_ = nullptr;

    // 表示用の、いまのデバッグカメラの状態(nullopt = 持たないシーン)。毎フレーム、シーンから伝えられる
    std::optional<bool> debugCameraState_;

    // シーン変更通知コールバック（オートセーブ用）
    std::function<void()> onModified_;

#ifdef USE_IMGUI
    // UIの状態変数（エディタのみで使用）

    // トラック追加時の仮名
    char newTrackName_[128] = "NewTrack";

    // イベント追加ポップアップの対象トラックインデックス
    int addEventTargetTrackIndex_ = -1;

    // 追加するイベントの発火時刻
    float addEventTime_ = 0.0f;

    // 選択中のイベント種別のインデックス（TimelineEventFactory::GetRegisteredEventNames()内）
    int selectedEventTypeIndex_ = 0;

    // 追加ポップアップの状態: TimelineEventFactory経由で生成した、
    // まだトラックに追加していない一時インスタンス（ポップアップ表示中はこの実体に対して
    // DrawCreationUI()でフィールドを編集する）
    std::unique_ptr<ITimelineEvent> pendingEvent_;

    // トラックのラベル列（名前+ボタン）の固定幅（ピクセル）
    // ルーラー・シークバー・各トラックレーンはこの幅ぶんオフセットして座標系を揃える
    static constexpr float kTrackLabelWidth = 130.0f;

    // トラック1本の高さ（ピクセル）
    static constexpr float kTrackHeight = 30.0f;
#endif
};

} // namespace AbsoluteEngine
