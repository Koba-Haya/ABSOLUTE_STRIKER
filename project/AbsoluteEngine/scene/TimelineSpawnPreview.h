#pragma once
#include <functional>
#include <memory>

#include "Transform.h"

namespace AbsoluteEngine {

class GameObject;
class ITimelineEvent;
class SpawnEvent;
class TimelineManager;

// エディタで、タイムラインの SpawnEvent を選んだときに、出現する敵の「仮のオブジェクト(プレビュー)」を
// シーンに出して、ギズモで動かせるようにするクラス。
//
// 分けた理由: 「プレビューの出し入れと、動かした結果のイベントへの書き戻し」は、タイムラインの再生とは別の関心事で、
// エディタ専用の機能のため。TimelineHost から切り離して、単独で理解・差し替えできるようにした。
// シーンへの追加と、エディタ上の選択は、呼び出し側から関数で渡してもらう。
// これで、このクラスは BaseScene(のオーバーライドされる追加処理)や EditorUIManager を知らずに済む。
class TimelineSpawnPreview {
public:
  // 仮のオブジェクトをシーンへ追加する関数(例: 派生シーンが上書きした追加処理を呼ぶ)
  using AddObjectFunc = std::function<void(const std::shared_ptr<GameObject> &)>;
  // エディタ上の選択を変える関数(nullptr で選択解除)
  using SelectObjectFunc = std::function<void(const std::shared_ptr<GameObject> &)>;

  // 毎フレーム、エディットモード中に呼ぶ。
  //   - 選択イベントが切り替わったら、古いプレビューを破棄し、SpawnEvent なら新しいプレビューを出す
  //   - プレビューが存在する間は、ギズモで動かした結果を、イベントの出現位置へ書き戻す
  // manager: 選択イベントと、発火時刻・レールの情報の元
  void Update(TimelineManager &manager, const AddObjectFunc &addObject, const SelectObjectFunc &selectObject);

private:
  // SpawnEvent の出現位置を世界座標で返す。レール座標の指定のときは、
  // 実際の出現(Fire)と同じ手順で、発火時刻のレール位置から変換する
  static Transform ResolvePlacement(const TimelineManager &manager, const SpawnEvent &spawnEvent);

  // 選択が変わったときの、古いプレビューの破棄と新しいプレビューの生成
  void SwitchPreview(TimelineManager &manager, ITimelineEvent *selectedEvent, const AddObjectFunc &addObject,
                     const SelectObjectFunc &selectObject);
  // プレビューを動かした結果を、イベントへ書き戻す
  void WriteBackToEvent(const TimelineManager &manager, ITimelineEvent *selectedEvent);

  // 仮のオブジェクトへの弱参照。他のイベントへ選択が移ったとき、古いものを Destroy してから新しく作る
  std::weak_ptr<GameObject> previewObject_;
  // 前フレームの選択イベント(選択の切り替えを検出するため)
  ITimelineEvent *prevSelectedEvent_ = nullptr;
};

} // namespace AbsoluteEngine
