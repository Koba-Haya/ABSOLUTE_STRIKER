// ============================================================
// TimelineTrack.h
// 役割: 同じ種類のイベントをまとめるコンテナ
//       時間の差分（スイープ）を見てイベントのFire/Rewindを制御する
//       各イベントの発火時刻と発火済みフラグは、このトラックが管理する
//       （イベント自身は「何をするか」だけを持ち、いつ発火するかは知らない）
// ============================================================
#pragma once
#include "ITimelineEvent.h"
#include <cstddef>
#include <vector>
#include <memory>
#include <string>

namespace AbsoluteEngine {

class TimelineTrack {
public:
    // FindEventIndex で見つからなかったときの戻り値
    static constexpr size_t kEventNotFound = static_cast<size_t>(-1);

    // トラック名（エディタUI表示用）
    const std::string& GetName() const { return trackName_; }
    void SetName(const std::string& name) { trackName_ = name; }

    // -----------------------------------------------------------
    // イベント管理
    // -----------------------------------------------------------

    // イベントを発火時刻 triggerTime（秒）で追加し、発火時刻の昇順に並べ直す
    // 同じ時刻のイベントは、追加した順を保つ
    void AddEvent(std::unique_ptr<ITimelineEvent> event, float triggerTime);

    // イベントを削除する（インデックス指定）
    void RemoveEvent(size_t index);

    // イベントの数を取得する
    size_t GetEventCount() const { return entries_.size(); }

    // 指定インデックスのイベントを取得する（非所有ポインタ。範囲外は nullptr）
    // インデックスは発火時刻の昇順。時刻の変更や追加・削除で変わるため、保持せず、使うたびに取得すること
    ITimelineEvent* GetEvent(size_t index) const;

    // 指定インデックスのイベントの発火時刻（秒）を取得する（範囲外は 0）
    float GetTriggerTime(size_t index) const;

    // イベントのインデックスを探す（このトラックに無ければ kEventNotFound）
    size_t FindEventIndex(const ITimelineEvent* event) const;

    // イベントの発火時刻を変更し、昇順の並びを保つ（このトラックに無ければ false）
    // 並びが変わるため、変更後は以前のインデックスが無効になる
    bool SetEventTriggerTime(const ITimelineEvent* event, float triggerTime);

    // -----------------------------------------------------------
    // 時間更新（スイープ判定）
    // -----------------------------------------------------------

    // previousTime から currentTime への時間変化を評価し、
    // 通過したイベントに対して Fire() または Rewind() を呼ぶ
    //
    // ガードレール: 連続シークでも過剰な発火が起きないよう、
    //              前後の時間の差分のみを評価するスイープ判定方式を採用
    void Update(float currentTime, float previousTime);

    // すべてのイベントを強制的にRewindする（Stop時などに使用）
    void RewindAll();

    // -----------------------------------------------------------
    // シリアライズ
    // -----------------------------------------------------------
    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

private:
    // イベントと、そのイベントに紐づくスケジュール情報の組
    struct Entry {
        std::unique_ptr<ITimelineEvent> event;
        // イベントが発火する時刻（秒）
        float triggerTime = 0.0f;
        // 発火済みフラグ（状態ベースのイベント管理に使用）
        // true: Fire()済み / false: まだ発火していない or Rewind()済み
        bool isFired = false;
    };

    // 発火時刻の昇順に並べ直す内部ヘルパー（同じ時刻は元の順を保つ）
    void SortEntries();

    std::string trackName_ = "NewTrack";

    // triggerTime の昇順でソート済みのイベントリスト
    std::vector<Entry> entries_;
};

} // namespace AbsoluteEngine
