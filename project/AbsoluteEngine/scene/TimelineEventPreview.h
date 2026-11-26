#pragma once
#include <functional>
#include <memory>

#include "Transform.h"

namespace AbsoluteEngine {

class GameObject;
class ITimelineEvent;
class TimelineManager;
struct TimelineEditorHandle;

// エディタで、タイムラインのイベントを選んだときに、そのイベントの「目印(仮のオブジェクト)」をシーンに出して、
// ギズモで動かせるようにするクラス(以前の TimelineSpawnPreview。SpawnEvent 専用ではなくなったので名前を変えた)。
//
// 分けた理由: 「目印の出し入れと、動かした結果のイベントへの書き戻し」は、タイムラインの再生とは別の関心事で、
// エディタ専用の機能のため。TimelineHost から切り離して、単独で理解・差し替えできるようにした。
// シーンへの追加と、エディタ上の選択は、呼び出し側から関数で渡してもらう。
// これで、このクラスは BaseScene(のオーバーライドされる追加処理)や EditorUIManager を知らずに済む。
//
// イベントの具体的な型は知らない。目印があるか、どこに何を置くかは、ITimelineEvent::GetEditorHandle で問い合わせ、
// 動かした結果は ITimelineEvent::OnEditorHandleEdited で伝える(どの成分を取り込むかは、イベントが決める)。
class TimelineEventPreview {
public:
  // 仮のオブジェクトをシーンへ追加する関数(例: 派生シーンが上書きした追加処理を呼ぶ)
  using AddObjectFunc = std::function<void(const std::shared_ptr<GameObject> &)>;
  // エディタ上の選択を変える関数(nullptr で選択解除)
  using SelectObjectFunc = std::function<void(const std::shared_ptr<GameObject> &)>;

  // 毎フレーム、エディットモード中に呼ぶ。
  //   - 選択イベントが切り替わったら、古い目印を破棄し、新しいイベントに目印があれば出す
  //   - 目印が存在する間は、ギズモで動かした結果を、イベントへ書き戻す
  // manager: 選択イベントと、発火時刻・レールの情報の元
  void Update(TimelineManager &manager, const AddObjectFunc &addObject, const SelectObjectFunc &selectObject);

private:
  // イベントに、目印を問い合わせる。発火時刻のレール位置などは、実際の発火と同じ手順で求める
  // 戻り値: 目印があれば true
  static bool QueryHandle(const TimelineManager &manager, const ITimelineEvent &event, TimelineEditorHandle &outHandle);

  // 選択が変わったときの、古い目印の破棄と新しい目印の生成
  void SwitchPreview(TimelineManager &manager, ITimelineEvent *selectedEvent, const AddObjectFunc &addObject,
                     const SelectObjectFunc &selectObject);
  // 目印を動かした結果を、イベントへ書き戻し、イベントが決めた置き場所へ目印を合わせ直す
  void WriteBackToEvent(const TimelineManager &manager, ITimelineEvent *selectedEvent);

  // 仮のオブジェクトへの弱参照。他のイベントへ選択が移ったとき、古いものを Destroy してから新しく作る
  std::weak_ptr<GameObject> previewObject_;
  // 前フレームの選択イベント(選択の切り替えを検出するため)
  ITimelineEvent *prevSelectedEvent_ = nullptr;
};

} // namespace AbsoluteEngine
