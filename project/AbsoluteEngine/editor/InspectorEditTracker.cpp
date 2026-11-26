#include "InspectorEditTracker.h"
#include "../scene/Component.h"
#include "../scene/IEditRecorder.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AbsoluteEngine {

namespace {
// 記録先を包み、部品が自分で記録したかを覚えておく窓口(Decorator: 記録の仕事は、包んだ相手へそのまま渡す)。
// 部品が自分で記録したフレームに、トラッカーが同じ編集をもう一度記録しないために使う
class RecordedFlagRecorder final : public IEditRecorder {
public:
  explicit RecordedFlagRecorder(IEditRecorder& inner) : inner_(inner) {}

  void RecordEdit(const nlohmann::json& before, const nlohmann::json& after) override {
    inner_.RecordEdit(before, after);
    recorded_ = true;
  }

  // このフレームに、部品が自分で記録したか
  bool HasRecorded() const { return recorded_; }

private:
  IEditRecorder& inner_; // 本来の記録先(所有しない)
  bool recorded_ = false;
};
} // namespace

void InspectorEditTracker::DrawAndRecord(IComponent& component, IEditRecorder& recorder) {
#ifdef USE_IMGUI
  // 前のフレームから操作が続いている部品なら、操作の始まりに覚えた状態を使う。
  // そうでなければ、描く前の今の状態を「編集前」にする(ゲームの進行で値が変わっても、描く直前に取り直すので、記録に混ざらない)
  const bool continuing = (editingComponent_ == &component);
  nlohmann::json before;
  if (!continuing) {
    component.Serialize(before);
  }

  // 部品のウィジェットをグループで囲み、グループ全体を 1 つの項目として、操作中かを調べられるようにする
  RecordedFlagRecorder flagged(recorder);
  ImGui::BeginGroup();
  component.DrawInspectorUI(flagged);
  ImGui::EndGroup();

  // 操作中(ドラッグ中、入力中など)は、まだ記録しない。操作の始まりなら、編集前の状態を預かる
  if (ImGui::IsItemActive()) {
    if (!continuing) {
      editingComponent_ = &component;
      editBefore_ = std::move(before);
    }
    return;
  }

  // ここに来たら、操作は終わっている(または、このフレームだけの単発の変更)
  const nlohmann::json& start = continuing ? editBefore_ : before;

  // 部品が自分で記録した編集は、二重に記録しない
  if (!flagged.HasRecorded()) {
    nlohmann::json after;
    component.Serialize(after);
    // 何も変わらなかった操作は、履歴に残さない
    if (after != start) {
      recorder.RecordEdit(start, after);
    }
  }

  if (continuing) {
    editingComponent_ = nullptr;
    editBefore_ = nlohmann::json();
  }
#else
  (void)component;
  (void)recorder;
#endif
}

} // namespace AbsoluteEngine
