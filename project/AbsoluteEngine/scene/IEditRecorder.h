#pragma once
#include "../../externals/nlohmann/json.hpp"

namespace AbsoluteEngine {

// 部品のインスペクタでの編集を、取り消し履歴へ記録するための窓口(インターフェース)。
//
// 作った理由: 部品(RailCameraComponent)が、取り消し履歴に記録するために、グローバル(以前の BaseScene::GetActiveScene)から
// 「今のシーン」を取り、コマンドマネージャを探していた。取り消しの記録は、エディタの責務なので、部品は「編集した」と伝えるだけにして、
// 履歴の作り方(どのオブジェクトの、どの部品の変更か)は、エディタが決める。
// 窓口は、エディタがインスペクタを描くとき(IComponent::DrawInspectorUI)に、引数で渡す。部品が保持しない。
class IEditRecorder {
public:
  virtual ~IEditRecorder() = default;

  // 編集で、部品の状態が before から after へ変わったことを記録する(Serialize した JSON を渡す)。
  // 編集を取り消すと before へ、やり直すと after へ戻る
  virtual void RecordEdit(const nlohmann::json& before, const nlohmann::json& after) = 0;
};

} // namespace AbsoluteEngine
