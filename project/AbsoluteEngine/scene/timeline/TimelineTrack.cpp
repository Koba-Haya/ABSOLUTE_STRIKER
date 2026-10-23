// ============================================================
// TimelineTrack.cpp
// イベントコンテナとスイープ判定ロジックの実装
// ============================================================
#include "TimelineTrack.h"
#include "TimelineEventFactory.h"
#include <algorithm>
#include <iostream>

namespace AbsoluteEngine {

void TimelineTrack::AddEvent(std::unique_ptr<ITimelineEvent> event, float triggerTime) {
    Entry entry;
    entry.event = std::move(event);
    entry.triggerTime = triggerTime;
    entries_.push_back(std::move(entry));
    SortEntries();
}

void TimelineTrack::RemoveEvent(size_t index) {
    if (index >= entries_.size()) return;

    // 削除前にRewindして状態をクリーンにする
    entries_[index].event->Rewind();
    entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(index));
}

ITimelineEvent* TimelineTrack::GetEvent(size_t index) const {
    return index < entries_.size() ? entries_[index].event.get() : nullptr;
}

float TimelineTrack::GetTriggerTime(size_t index) const {
    return index < entries_.size() ? entries_[index].triggerTime : 0.0f;
}

size_t TimelineTrack::FindEventIndex(const ITimelineEvent* event) const {
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].event.get() == event) return i;
    }
    return kEventNotFound;
}

bool TimelineTrack::SetEventTriggerTime(const ITimelineEvent* event, float triggerTime) {
    const size_t index = FindEventIndex(event);
    if (index == kEventNotFound) return false;

    entries_[index].triggerTime = triggerTime;
    SortEntries();
    return true;
}

void TimelineTrack::SortEntries() {
    // 同じ時刻のイベントは追加した順を保つため stable_sort を使う
    std::stable_sort(entries_.begin(), entries_.end(),
        [](const Entry& a, const Entry& b) {
            return a.triggerTime < b.triggerTime;
        });
}

void TimelineTrack::Update(float currentTime, float previousTime) {
    if (entries_.empty()) return;

    const bool isForward = (currentTime >= previousTime); // 前進方向かどうか

    if (isForward) {
        // 前進方向: triggerTime が currentTime 以下かつ未発火のイベントを Fire()
        // 区間判定ではなく isFired フラグで管理するため、シークで飛ばした場合も正確に発火する
        for (auto& entry : entries_) {
            if (entry.triggerTime <= currentTime && !entry.isFired) {
                entry.event->Fire();
                entry.isFired = true;
            }
        }
    } else {
        // 後退方向（シーク巻き戻し）: triggerTime が currentTime を超えかつ発火済みのイベントを Rewind()
        // ガードレール: 逆順でRewindすることで依存関係のある場合も安全に処理できる
        for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
            if (it->triggerTime > currentTime && it->isFired) {
                it->event->Rewind();
                it->isFired = false;
            }
        }
    }
}

void TimelineTrack::RewindAll() {
    // 逆順でRewindして依存関係の問題を防ぐ
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        it->event->Rewind();
        // 発火済みフラグもリセットして次の再生に備える
        it->isFired = false;
    }
}

void TimelineTrack::Serialize(nlohmann::json& j) const {
    j["trackName"] = trackName_;

    nlohmann::json eventsJson = nlohmann::json::array();
    for (const auto& entry : entries_) {
        nlohmann::json eventJson;
        // イベントの種別を保存（デシリアライズ時に適切なクラスを選択するため）
        eventJson["eventType"]   = entry.event->GetEventType();
        eventJson["triggerTime"] = entry.triggerTime;
        entry.event->Serialize(eventJson);
        eventsJson.push_back(eventJson);
    }
    j["events"] = eventsJson;
}

void TimelineTrack::Deserialize(const nlohmann::json& j) {
    trackName_ = j.value("trackName", "NewTrack");
    entries_.clear();

    if (!j.contains("events") || !j["events"].is_array()) return;

    for (const auto& eventJson : j["events"]) {
        const std::string eventType = eventJson.value("eventType", "");

        // イベント種別に応じてインスタンスを生成する。
        // TimelineEventFactory にエンジン組み込み種別（SpawnEvent）と
        // Application層が実行時にRegister()した種別（FormationSpawnEvent等）が
        // 両方登録されているため、ここでは名前解決するだけでよい。
        std::unique_ptr<ITimelineEvent> event = TimelineEventFactory::GetInstance().Create(eventType);

        if (event) {
            event->Deserialize(eventJson);

            Entry entry;
            entry.event = std::move(event);
            entry.triggerTime = eventJson.value("triggerTime", 0.0f);
            entries_.push_back(std::move(entry));
        } else {
            std::cerr << "[TimelineTrack] 未知のイベント種別: \"" << eventType << "\"" << std::endl;
        }
    }

    // デシリアライズ後はソートして時間順を保証する
    SortEntries();
}

} // namespace AbsoluteEngine
