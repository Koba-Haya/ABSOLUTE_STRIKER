#pragma once
#include "CloudImpact.h"
#include "Method.h"

// 雲の着地の反応(CloudImpact)の確認用 ImGui。Debug / Development のみ(USE_IMGUI が無ければ何もしない)。
// 着地点・強さを指定して「Cloud Impact」ボタンで反応を起こせる。死亡演出を毎回見なくても、雲の反応だけを確認できる。
//
// クラスにした理由: 着地点と強さは、呼び出しをまたいで保持する必要がある。以前は関数内 static(グローバルな状態)に
// 持っていたので、窓の持ち主(GameScene)が、このクラスのインスタンスをメンバとして持つ形にした。
class CloudImpactDebugUi {
public:
  // defaultPosition: 着地点の初期値(自機の位置など)。最初の呼び出しで、着地点をこの値にする
  void Draw(CloudImpact &impact, const Vector3 &defaultPosition);

private:
  Vector3 position_{0.0f, 0.0f, 0.0f}; // ボタンで反応を起こすときの着地点
  float strength_ = 1.0f;              // ボタンで反応を起こすときの強さ
  bool initialized_ = false;           // 着地点を、初期値で埋めたか
};
