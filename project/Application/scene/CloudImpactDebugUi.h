#pragma once
#include "CloudImpact.h"
#include "Method.h"

// 雲の着地の反応(CloudImpact)の確認用 ImGui。Debug / Development のみ(USE_IMGUI が無ければ何もしない)。
// 着地点・強さを指定して「Cloud Impact」ボタンで反応を起こせる。死亡演出を毎回見なくても、雲の反応だけを確認できる。
namespace CloudImpactDebugUi {
// defaultPosition: 着地点の初期値(自機の位置など)。position / strength は呼び出しをまたいで保持される
void Draw(CloudImpact &impact, const Vector3 &defaultPosition);
} // namespace CloudImpactDebugUi
