#pragma once
#include "PlayerDeathDust.h"
#include "ScaleDust.h"

// 鱗粉(ScaleDust)の調整用 ImGui。Debug / Development のみ(USE_IMGUI が無ければ何もしない。Release は ImGui なし)。
// 量・寿命・大きさ・色・重力・風などを動かして、見た目を決める。気に入った値は、ScaleDust.h / PlayerDeathDust.h の既定値へ写す。
namespace ScaleDustDebugUi {
// 鱗粉そのものの設定(GameOver など、どこで使っても共通)
void Draw(ScaleDustSettings &settings);
// 死亡の崩壊の鱗粉: 発生の量・場所の設定と、鱗粉そのものの設定
void Draw(PlayerDeathDust &deathDust);
} // namespace ScaleDustDebugUi
