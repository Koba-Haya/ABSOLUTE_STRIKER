#include "TimelineEventPreview.h"

#include "GameObject.h"
#include "SceneSerializer.h"
#include "timeline/ITimelineEvent.h"
#include "timeline/PrefabRegistry.h"
#include "timeline/TimelineManager.h"

namespace AbsoluteEngine {

bool TimelineEventPreview::QueryHandle(const TimelineManager &manager, const ITimelineEvent &event, TimelineEditorHandle &outHandle) {
  // 発火時刻の文脈(レール位置など)を作り、実際の発火と同じ手順で、目印の置き場所を求めてもらう
  float triggerTime = 0.0f;
  manager.GetEventTriggerTime(&event, triggerTime);
  return event.GetEditorHandle(manager.MakeFireContext(triggerTime), outHandle);
}

void TimelineEventPreview::SwitchPreview(TimelineManager &manager, ITimelineEvent *selectedEvent,
                                         const AddObjectFunc &addObject, const SelectObjectFunc &selectObject) {
  // 古い目印を破棄し、エディタの選択も解除する
  if (auto prevPreview = previewObject_.lock()) {
    prevPreview->Destroy();
    selectObject(nullptr);
  }
  previewObject_.reset();
  prevSelectedEvent_ = selectedEvent;

  // 新しいイベントが選択されていて、目印を持つなら、目印のオブジェクトを生成する(イベントの種類は問わない)
  if (selectedEvent == nullptr) return;
  TimelineEditorHandle handle;
  if (!QueryHandle(manager, *selectedEvent, handle)) return;

  const std::string path = PrefabRegistry::GetInstance().ResolveId(handle.prefabId);
  // 窓口の束が無いと、プレハブの部品を作れない(通常は、シーンが設定している)
  const ComponentDeps *deps = manager.GetDeps();
  if (path.empty() || !deps) return;

  // プレハブをロードして仮オブジェクトとして生成する
  auto previewObj = SceneSerializer::LoadPrefab(path, *deps);
  if (!previewObj) return;
  // プレビューフラグを立ててシリアライズ対象から除外する
  previewObj->SetTimelinePreview(true);
  // イベントが決めた置き場所に置く(レール座標の指定なら、実際の出現と同じ手順で世界座標へ変換した位置)
  previewObj->SetTransform(handle.placement);
  // シーンに追加してギズモ操作対象に登録する
  addObject(previewObj);
  selectObject(previewObj);
  previewObject_ = previewObj;
}

void TimelineEventPreview::WriteBackToEvent(const TimelineManager &manager, ITimelineEvent *selectedEvent) {
  auto previewObj = previewObject_.lock();
  if (!previewObj || !selectedEvent) return;

  // ギズモまたはインスペクタで変更された Transform を、イベントへ伝える
  // (どの成分を取り込むか。例: レール座標の指定のときに位置を取り込まない、の判断は、イベント自身が行う)
  selectedEvent->OnEditorHandleEdited(previewObj->GetTransform());

  // イベントが決めた置き場所へ、目印を合わせ直す。
  // レール座標の指定のときは、ギズモで位置を動かしても、毎フレーム、レール座標から求めた位置へ戻る
  TimelineEditorHandle handle;
  if (QueryHandle(manager, *selectedEvent, handle)) {
    previewObj->SetTransform(handle.placement);
  }
}

void TimelineEventPreview::Update(TimelineManager &manager, const AddObjectFunc &addObject,
                                  const SelectObjectFunc &selectObject) {
  ITimelineEvent *currentSelectedEvent = manager.GetSelectedEvent();

  // 選択イベントが切り替わったか、または選択解除された場合は、目印を作り直す
  if (prevSelectedEvent_ != currentSelectedEvent) {
    SwitchPreview(manager, currentSelectedEvent, addObject, selectObject);
  }

  // 目印が存在する場合は、ギズモ操作を、毎フレーム、イベントへ書き戻す
  WriteBackToEvent(manager, currentSelectedEvent);
}

} // namespace AbsoluteEngine
