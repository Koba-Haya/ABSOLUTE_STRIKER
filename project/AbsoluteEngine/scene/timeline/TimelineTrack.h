// ============================================================
// TimelineTrack.h
// 役割: 同じ種類のイベントをまとめるコンテナ
//       時間の差分（スイープ）を見てイベントのFire/Rewindを制御する
// ============================================================
#pragma once
#include "ITimelineEvent.h"
#include <vector>
#include <memory>
#include <string>

namespace AbsoluteEngine {

class TimelineTrack {
public:
    // トラック名（エディタUI表示用）
    std::string trackName_ = "NewTrack";

    // -----------------------------------------------------------
    // イベント管理
    // -----------------------------------------------------------

    // イベントを追加し、triggerTime_ で昇順ソートする
    void AddEvent(std::unique_ptr<ITimelineEvent> event);

    // イベントを削除する（インデックス指定）
    void RemoveEvent(size_t index);

    // イベントリストを取得（読み取り専用）
    const std::vector<std::unique_ptr<ITimelineEvent>>& GetEvents() const { return events_; }

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
    // triggerTime_ の昇順でソート済みのイベントリスト
    std::vector<std::unique_ptr<ITimelineEvent>> events_;

    // イベントリストを triggerTime_ の昇順でソートする内部ヘルパー
    void SortEvents();
};

} // namespace AbsoluteEngine
