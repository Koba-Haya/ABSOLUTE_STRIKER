// ============================================================
// TimelineManager.h
// 役割: タイムライン全体の再生/停止/シークを管理するシステムクラス
//       BaseSceneが所有し、シーン固有のタイムラインデータを管理する
// ============================================================
#pragma once
#include "TimelineTrack.h"
#include "ITimelineEvent.h"
#include "../../camera/ITimelineCamera.h"
#include "../../Type/Transform.h"
#include "../../../externals/nlohmann/json.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace AbsoluteEngine {

// 再生状態
enum class TimelinePlayState {
    Stopped,  // 停止
    Playing,  // 再生中
    Paused    // 一時停止中
};

class TimelineManager {
public:
    TimelineManager() = default;
    ~TimelineManager() = default;

    // -----------------------------------------------------------
    // 初期化・終了
    // -----------------------------------------------------------

    // タイムライン全体の長さ（秒）を取得する
    // ITimelineCamera（レール実装）を最初から最後まで等速で走破するのに必要な秒数を
    // 毎回動的に算出する。手動設定は廃止し、レールの形状・速度に完全追従させる
    // カメラ未接続、またはレール長・速度が0の場合はフォールバック値を返す
    float GetDuration() const;

    // -----------------------------------------------------------
    // ダーティフラグ管理（未保存変更の追跡）
    // -----------------------------------------------------------

    // データが変更されたことをマークする
    void SetDirty(bool dirty = true) { isDirty_ = dirty; }

    // 未保存の変更があるか確認する
    bool IsDirty() const { return isDirty_; }

    // シーン読み込み時にタイムラインカメラ（ITimelineCamera実装）を関連付ける
    // タイムラインはカメラの進行度をこのポインタ経由で同期する
    void SetCamera(ITimelineCamera* camera) { timelineCamera_ = camera; }

    // ComputeSpawnAnchorTransform()でカメラ前方に自動配置する際の距離（ワールド単位）を設定する
    // プレイヤーとカメラの位置関係・狙わせたい距離感はゲームごとに異なるため、
    // 既定値（80.0f）はあくまで暫定値。ゲーム側の実際のプレイヤー/照準距離に合わせて呼び出すこと
    void SetSpawnAheadDistance(float distance) { spawnAheadDistance_ = distance; }

    // -----------------------------------------------------------
    // スポーン位置の自動算出
    // -----------------------------------------------------------

    // 発火時刻に対応するレール上の想定プレイヤー位置から、前方向へ少し奥へ進めた位置を算出する
    // SpawnEvent追加時のデフォルト出現位置に使う（座標を手入力する代わりに自動配置する）
    // カメラ未接続の場合は原点のTransformを返す
    Transform ComputeSpawnAnchorTransform(float triggerTime) const;

    // -----------------------------------------------------------
    // 再生制御
    // -----------------------------------------------------------

    // 再生開始（先頭または現在位置から）
    void Play();

    // 一時停止
    void Pause();

    // 停止（先頭に戻り、全イベントをRewind）
    void Stop();

    // 現在の再生状態を取得する
    TimelinePlayState GetPlayState() const { return playState_; }

    // -----------------------------------------------------------
    // シーク（時間ジャンプ）
    // -----------------------------------------------------------

    // 指定した時間に直接ジャンプする
    // - 前進方向: 通過したイベントをFire()
    // - 後退方向: 通過したイベントをRewind()
    // - カメラのprogress_を即座に同期する
    void Seek(float targetTime);

    // 現在の再生時間を取得する
    float GetCurrentTime() const { return currentTime_; }

    // -----------------------------------------------------------
    // 毎フレーム更新
    // -----------------------------------------------------------

    // DeltaTimeを加算して時間を進め、イベントを評価する
    // カメラのprogress_も同期して更新する
    void Update(float deltaTime);

    // -----------------------------------------------------------
    // トラック管理
    // -----------------------------------------------------------

    // トラックを追加する
    void AddTrack(std::unique_ptr<TimelineTrack> track);

    // トラックを削除する（インデックス指定）
    void RemoveTrack(size_t index);

    // トラックリストを取得する（読み取り専用）
    const std::vector<std::unique_ptr<TimelineTrack>>& GetTracks() const { return tracks_; }

    // 指定インデックスのトラックを書き込み可能な形で取得する（エディタUI用）
    // トラックの追加・削除はAddTrack/RemoveTrackに限定し、リスト自体は公開しない
    // 範囲外のインデックスの場合は nullptr を返す（所有権は渡さない）
    TimelineTrack* GetTrack(size_t index) {
        return index < tracks_.size() ? tracks_[index].get() : nullptr;
    }

    // -----------------------------------------------------------
    // イベント選択状態管理（タスク14）
    // -----------------------------------------------------------

    // 現在選択されているイベントを取得する（非所有ポインタ）
    ITimelineEvent* GetSelectedEvent() const { return selectedEvent_; }

    // イベントを選択する（nullptr を渡すと選択解除）
    void SetSelectedEvent(ITimelineEvent* event) { selectedEvent_ = event; }

    // 選択中のイベントが削除される際に安全に nullptr にクリアする
    // RemoveTrack / RemoveEvent の前に呼ぶことでダングリングポインタを防ぐ
    void ClearSelectedEventIfMatch(ITimelineEvent* event) {
        if (selectedEvent_ == event) {
            selectedEvent_ = nullptr;
        }
    }

    // -----------------------------------------------------------
    // シリアライズ（ファイル保存/読み込み）
    // -----------------------------------------------------------

    // タイムラインデータをJSONファイルに保存する
    bool SaveToFile(const std::string& filePath) const;

    // JSONファイルからタイムラインデータを読み込む
    bool LoadFromFile(const std::string& filePath);

    // タイムラインデータをJSON文字列にシリアライズする（Undo/Redo用スナップショット）
    std::string SerializeToString() const;

    // JSON文字列からタイムラインデータを復元する（Undo/Redo用）
    // 復元後は必ず selectedEvent_ を nullptr にリセットする（ダングリングポインタ防止）
    bool LoadFromString(const std::string& jsonStr);

private:
    // カメラの進行度を現在時間から同期するヘルパー
    void SyncCameraProgress();

    // 全イベントを強制的にRewindするヘルパー（Stop時用）
    void RewindAllEvents();

private:
    // RailCameraComponent未接続時に使うフォールバックの長さ（秒）
    static constexpr float kFallbackDuration_ = 60.0f;

    // 現在の再生時間（秒）
    float currentTime_ = 0.0f;

    // 直前フレームの再生時間（スイープ判定に使用）
    float previousTime_ = 0.0f;

    // 再生状態
    TimelinePlayState playState_ = TimelinePlayState::Stopped;

    // トラックのリスト
    std::vector<std::unique_ptr<TimelineTrack>> tracks_;

    // カメラへの非所有ポインタ（実体の寿命はBaseSceneが管理）
    ITimelineCamera* timelineCamera_ = nullptr;

    // ComputeSpawnAnchorTransform()で使う、カメラ前方への配置距離（SetSpawnAheadDistanceで変更可）
    float spawnAheadDistance_ = 80.0f;

    // 現在選択中のイベントへの非所有ポインタ（タスク14）
    // 所有権は各 TimelineTrack が持つ。削除時は ClearSelectedEventIfMatch で nullptr に戻す
    ITimelineEvent* selectedEvent_ = nullptr;

    // 未保存の変更があることを示すダーティフラグ
    // AddTrack/RemoveTrack 等の変更操作でtrueになり、SaveToFile後にfalseにリセットする
    // SaveToFile() は論理的にconstだが、保存完了のマーキングのため mutable にする
    mutable bool isDirty_ = false;
};

} // namespace AbsoluteEngine
