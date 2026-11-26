#pragma once
#include "../../externals/nlohmann/json.hpp"

namespace AbsoluteEngine {

class IComponent;
class IEditRecorder;

// 部品(IComponent)のインスペクタを描き、その編集を取り消し履歴へ記録する役(エディタ側)。
//
// 作った理由: 以前は、エディタ(EditorUIManager)が、ライトなどの部品ごとに「編集が始まった / 終わった」を調べて、
// 取り消し履歴を作っていた(部品の種類ごとの分岐)。部品は DrawInspectorUI を override して描くだけにし、
// 「編集の前後の状態(Serialize した JSON)を比べて、変わっていれば記録する」処理を、ここで全部品に共通で行う。
// これにより、エディタは部品の具体的な型を知らずに、基底(IComponent)の参照だけで、描画と記録ができる。
//
// 編集の区切り: 部品のウィジェット全体を ImGui のグループで囲み、グループ内のどれかを操作中(ドラッグ中など)の間は、
// 1 つの編集として扱う。操作が終わったフレーム(またはチェックボックスなどの単発の変更)で、前後を比べて記録する。
//
// 注意:
// - Serialize を持たない部品(何も書き出さない部品)は、前後が常に同じなので、記録されない。
// - 部品が自分で recorder に記録した(RailCameraComponent など)フレームは、二重に記録しないよう、ここでは記録しない。
// - 編集中の部品は 1 つだけ(ImGui で同時に操作できる項目は 1 つのため)。
class InspectorEditTracker {
public:
  // component のインスペクタを描き、編集で状態が変わっていたら recorder へ記録する。
  // component: 描く部品, recorder: 記録先(どのオブジェクトのどの部品かは、渡す側が決める)
  // 注意: ImGui のウィンドウの中で、毎フレーム呼ぶこと(編集の始まりと終わりを、フレームをまたいで追うため)
  void DrawAndRecord(IComponent& component, IEditRecorder& recorder);

private:
  // 操作中の部品(なければ nullptr)。比べるためだけに使い、参照先には触らない
  const IComponent* editingComponent_ = nullptr;
  // 操作中の部品の、操作が始まる前の状態
  nlohmann::json editBefore_;
};

} // namespace AbsoluteEngine
