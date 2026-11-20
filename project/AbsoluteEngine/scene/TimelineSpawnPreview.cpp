#include "TimelineSpawnPreview.h"

#include "GameObject.h"
#include "SceneSerializer.h"
#include "timeline/PrefabRegistry.h"
#include "timeline/SpawnEvent.h"
#include "timeline/TimelineManager.h"

namespace AbsoluteEngine {

Transform TimelineSpawnPreview::ResolvePlacement(const TimelineManager &manager, const SpawnEvent &spawnEvent) {
  Transform placed = spawnEvent.spawnTransform_;

  // 実際の出現(Fire)と同じ手順で、発火時刻のレール位置から世界座標を求める
  float triggerTime = 0.0f;
  manager.GetEventTriggerTime(&spawnEvent, triggerTime);
  RailFrame unusedFrame{};
  spawnEvent.ResolveSpawnTransform(manager.MakeFireContext(triggerTime), placed, unusedFrame);
  return placed;
}

void TimelineSpawnPreview::SwitchPreview(TimelineManager &manager, ITimelineEvent *selectedEvent,
                                         const AddObjectFunc &addObject, const SelectObjectFunc &selectObject) {
  // 古いプレビューオブジェクトを破棄し、エディタの選択も解除する
  if (auto prevPreview = previewObject_.lock()) {
    prevPreview->Destroy();
    selectObject(nullptr);
  }
  previewObject_.reset();

  // 新しいイベントが選択されていて、それが SpawnEvent なら、プレビューオブジェクトを生成する
  if (selectedEvent != nullptr) {
    if (auto *spawnEv = dynamic_cast<SpawnEvent *>(selectedEvent)) {
      const std::string path = PrefabRegistry::GetInstance().ResolveId(spawnEv->prefabId_);
      if (!path.empty()) {
        // プレハブをロードして仮オブジェクトとして生成する
        auto previewObj = SceneSerializer::LoadPrefab(path);
        if (previewObj) {
          // プレビューフラグを立ててシリアライズ対象から除外する
          previewObj->SetTimelinePreview(true);
          // スポーン位置に配置する(レール座標の指定なら、実際の出現と同じ手順で世界座標へ変換した位置)
          previewObj->GetTransform() = ResolvePlacement(manager, *spawnEv);
          // シーンに追加してギズモ操作対象に登録する
          addObject(previewObj);
          selectObject(previewObj);
          previewObject_ = previewObj;
        }
      }
    }
  }
  prevSelectedEvent_ = selectedEvent;
}

void TimelineSpawnPreview::WriteBackToEvent(const TimelineManager &manager, ITimelineEvent *selectedEvent) {
  auto previewObj = previewObject_.lock();
  if (!previewObj) return;

  auto *spawnEv = dynamic_cast<SpawnEvent *>(selectedEvent);
  if (!spawnEv) return;

  // ギズモまたはインスペクタで変更された Transform をイベントの値に書き戻す
  if (spawnEv->useRail_) {
    // レール座標の指定のときは、位置はレール座標(ahead, lateral, vertical)だけで決まる。
    // ギズモで動かしても位置は書き戻さず(世界座標へ戻すと、レール座標の指定が崩れる)、
    // 回転とスケールだけを書き戻す。プレビューの位置は、毎フレーム、レール座標から求め直す
    spawnEv->spawnTransform_.rotate = previewObj->GetTransform().rotate;
    spawnEv->spawnTransform_.scale = previewObj->GetTransform().scale;
    previewObj->GetTransform().translate = ResolvePlacement(manager, *spawnEv).translate;
  } else {
    spawnEv->spawnTransform_ = previewObj->GetTransform();
  }
}

void TimelineSpawnPreview::Update(TimelineManager &manager, const AddObjectFunc &addObject,
                                  const SelectObjectFunc &selectObject) {
  ITimelineEvent *currentSelectedEvent = manager.GetSelectedEvent();

  // 選択イベントが切り替わったか、または選択解除された場合は、プレビューを作り直す
  if (prevSelectedEvent_ != currentSelectedEvent) {
    SwitchPreview(manager, currentSelectedEvent, addObject, selectObject);
  }

  // プレビューオブジェクトが存在する場合は、ギズモ操作を SpawnEvent::spawnTransform_ に対して毎フレーム書き戻す
  WriteBackToEvent(manager, currentSelectedEvent);
}

} // namespace AbsoluteEngine
